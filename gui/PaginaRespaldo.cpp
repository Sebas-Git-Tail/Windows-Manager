#include "PaginaRespaldo.h"
#include "Tema.h"
#include "ui_PaginaRespaldo.h"

#include "ConfigServidor.h"
#include "Herramientas.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSysInfo>
#include <QSettings>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

using respaldo::Final;

namespace {

// Datos del servidor: los de servidor.json (ConfigServidor, igual que Common/Servidor.sh en Linux).
// Primero la red de casa y, si no contesta, Tailscale.
QStringList servidores() { return ConfigServidor::instancia().ipsSsh(); }
QString usuario() { return ConfigServidor::instancia().usuario(); }
QString carpetaRespaldos() { return ConfigServidor::instancia().carpetaRespaldos(); }
const QString ClaveNombre = QStringLiteral("respaldo/nombre");
const QString ClaveComputadora = QStringLiteral("respaldo/computadora");
const QString ClaveDestinoConfirmado = QStringLiteral("respaldo/destinoConfirmado");
const QString ClaveUltimo = QStringLiteral("respaldo/ultimo");          // fecha y hora del último respaldo real
const QString ClaveExcluidas = QStringLiteral("respaldo/excluidas");    // carpetas del usuario que no se copian
constexpr int EsperaServidorMs = 2000;

QSettings ajustes()
{
    return QSettings(Herramientas::archivoAjustes(), QSettings::IniFormat);
}

// «2026-11-26_10-05» → «26/11/2026 10:05»
QString fechaLegible(const QString &nombre)
{
    const QDateTime fecha = respaldo::fechaDeNombre(nombre);
    return fecha.isValid() ? fecha.toString(QStringLiteral("dd/MM/yyyy HH:mm")) : nombre;
}

} // namespace

PaginaRespaldo::PaginaRespaldo(QWidget *parent)
    : QWidget(parent), ui(new Ui::PaginaRespaldo)
{
    ui->setupUi(this);
    setWindowTitle(tr("Respaldo"));
    ui->barraProgreso->hide();
    ui->etiquetaUltimo->setProperty("secundario", true);
    ui->botonEmpezar->setIcon(tema::icono(QStringLiteral("subir")));
    ui->botonRestaurar->setIcon(tema::icono(QStringLiteral("restaurar")));
    ui->botonVersiones->setIcon(tema::icono(QStringLiteral("buscar")));
    ui->botonAgregarExclusion->setIcon(tema::icono(QStringLiteral("agregar")));
    ui->botonQuitarExclusion->setIcon(tema::icono(QStringLiteral("borrar")));

    const QSettings guardado = ajustes();
    ui->editPersona->setText(guardado.value(ClaveNombre).toString());
    // Se propone el nombre del equipo, pero una vez guardado es fijo: si cambiara solo cuando
    // alguien renombra la computadora, el respaldo empezaría en otra carpeta
    ui->editComputadora->setText(guardado.value(ClaveComputadora, QSysInfo::machineHostName()).toString());
    const auto alEditar = [this] {
        mostrarDescripcion();
        guardarNombres();
        // Las versiones anteriores que se buscaron eran de otra carpeta
        while (ui->comboOrigen->count() > 1)
            ui->comboOrigen->removeItem(1);
    };
    connect(ui->editPersona, &QLineEdit::textChanged, this, alEditar);
    connect(ui->editComputadora, &QLineEdit::textChanged, this, alEditar);

    ui->comboOrigen->addItem(tr("El último respaldo"), QString());
    ui->etiquetaExclusiones->setText(
        tr("Siempre quedan afuera AppData, OneDrive, el registro de Windows (NTUSER), .ssh, .cache, .java, Games, "
           "VirtualBox VMs, .npm y .ollama. Acá podés sumar carpetas tuyas que no quieras copiar al servidor "
           "(lo que ya se había copiado de ellas queda en el servidor, no se borra)."));
    cargarExclusiones();
    mostrarUltimo();

    connect(ui->botonEmpezar, &QPushButton::clicked, this, &PaginaRespaldo::empezar);
    connect(ui->editContrasena, &QLineEdit::returnPressed, this, [this] {
        if (ui->pestanas->currentWidget() == ui->pestanaRestaurar)
            restaurar();
        else if (ui->pestanas->currentWidget() == ui->pestanaRespaldar)
            empezar();
    });
    connect(ui->botonCancelar, &QPushButton::clicked, this, &PaginaRespaldo::cancelar);
    connect(ui->botonRestaurar, &QPushButton::clicked, this, &PaginaRespaldo::restaurar);
    connect(ui->botonVersiones, &QPushButton::clicked, this, &PaginaRespaldo::buscarVersiones);
    connect(ui->botonRestaurarCarpeta, &QPushButton::clicked, this, [this] { elegirParaRestaurar(true); });
    connect(ui->botonRestaurarArchivo, &QPushButton::clicked, this, [this] { elegirParaRestaurar(false); });
    connect(ui->botonAgregarExclusion, &QPushButton::clicked, this, &PaginaRespaldo::agregarExclusion);
    connect(ui->listaExclusiones, &QListWidget::itemSelectionChanged, this,
            [this] { ui->botonQuitarExclusion->setEnabled(!ui->listaExclusiones->selectedItems().isEmpty()); });
    connect(ui->botonQuitarExclusion, &QPushButton::clicked, this, [this] {
        qDeleteAll(ui->listaExclusiones->selectedItems());
        guardarExclusiones();
    });

    proceso.setProcessChannelMode(QProcess::MergedChannels);
    connect(&proceso, &QProcess::readyReadStandardOutput, this, &PaginaRespaldo::leerSalida);
    connect(&proceso, &QProcess::finished, this, &PaginaRespaldo::terminoProceso);
    connect(&proceso, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        tema::mostrarEstado(ui->etiquetaEstado, tr("No se pudo ejecutar rsync: %1").arg(proceso.errorString()), tema::Estado::Error);
        terminar();
    });
}

PaginaRespaldo::~PaginaRespaldo()
{
    delete ui;
}

void PaginaRespaldo::showEvent(QShowEvent *evento)
{
    QWidget::showEvent(evento);
    mostrarDescripcion();
}

void PaginaRespaldo::mostrarDescripcion()
{
    const QString aviso = tr("Es una copia espejo: lo que borraste de la computadora también se borra del respaldo, "
                             "pero lo que se borra o cambia queda guardado %1 días en «versiones anteriores».")
                              .arg(respaldo::DiasAnteriores);
    ui->etiquetaDescripcion->setText(persona().isEmpty() || computadora().isEmpty()
        ? tr("Copia tu carpeta personal al servidor, a la carpeta de la persona. %1").arg(aviso)
        : tr("Copia tu carpeta personal al servidor, a %1. %2").arg(rutaEnServidor(), aviso));
}

void PaginaRespaldo::mostrarUltimo()
{
    const QDateTime ultimo = QDateTime::fromString(ajustes().value(ClaveUltimo).toString(), Qt::ISODate);
    if (!ultimo.isValid()) {
        ui->etiquetaUltimo->setText(tr("Todavía no hiciste ningún respaldo desde esta computadora."));
        return;
    }
    const QString dia = QLocale(QLocale::Spanish).toString(ultimo.date(), QStringLiteral("dddd d/M/yyyy"));
    ui->etiquetaUltimo->setText(tr("Último respaldo: %1 a las %2.").arg(dia, ultimo.toString(QStringLiteral("HH:mm"))));
}

QString PaginaRespaldo::persona() const
{
    return respaldo::normalizarPersona(ui->editPersona->text());
}

QString PaginaRespaldo::computadora() const
{
    return respaldo::validarComputadora(ui->editComputadora->text());
}

// La carpeta de destino lleva la persona y el nombre de la computadora
QString PaginaRespaldo::rutaEnServidor() const
{
    return carpetaRespaldos() + persona() + u'/' + computadora() + u'/';
}

// Se guardan apenas se escriben, para que sigan ahí al volver a abrir la app. Haberlos escrito
// no confirma el destino: eso lo hace confirmarDestino() antes de la primera copia real.
void PaginaRespaldo::guardarNombres()
{
    QSettings guardado = ajustes();
    if (persona().isEmpty())
        guardado.remove(ClaveNombre);
    else
        guardado.setValue(ClaveNombre, persona());
    if (computadora().isEmpty())
        guardado.remove(ClaveComputadora);
    else
        guardado.setValue(ClaveComputadora, computadora());
}

// ── Qué no se copia ──

QStringList PaginaRespaldo::extras() const
{
    QStringList lista;
    for (int i = 0; i < ui->listaExclusiones->count(); ++i)
        lista << ui->listaExclusiones->item(i)->data(Qt::UserRole).toString();
    return lista;
}

void PaginaRespaldo::cargarExclusiones()
{
    const QStringList guardadas = ajustes().value(ClaveExcluidas).toStringList();
    for (const QString &relativa : guardadas) {
        if (respaldo::normalizarExclusion(relativa).isEmpty())
            continue;
        auto *item = new QListWidgetItem(tema::icono(QStringLiteral("carpeta")), QDir::toNativeSeparators(relativa),
                                         ui->listaExclusiones);
        item->setData(Qt::UserRole, relativa);
    }
}

void PaginaRespaldo::guardarExclusiones()
{
    QSettings guardado = ajustes();
    if (extras().isEmpty())
        guardado.remove(ClaveExcluidas);
    else
        guardado.setValue(ClaveExcluidas, extras());
}

void PaginaRespaldo::agregarExclusion()
{
    const QString carpeta = QFileDialog::getExistingDirectory(this, tr("Carpeta que no se copia"), QDir::homePath());
    if (carpeta.isEmpty())
        return;
    bool ok = false;
    const QString relativa = respaldo::rutaRelativaSegura(QDir::home().relativeFilePath(carpeta), &ok);
    if (!ok || relativa.isEmpty() || QDir::home().relativeFilePath(carpeta).startsWith(QStringLiteral(".."))) {
        tema::mostrarEstado(ui->etiquetaEstado, tr("Tiene que ser una carpeta de adentro de tu carpeta personal."), tema::Estado::Aviso);
        return;
    }
    if (extras().contains(relativa))
        return;
    auto *item = new QListWidgetItem(tema::icono(QStringLiteral("carpeta")), QDir::toNativeSeparators(relativa), ui->listaExclusiones);
    item->setData(Qt::UserRole, relativa);
    guardarExclusiones();
    ui->etiquetaEstado->clear();
}

// ── Confirmaciones ──

// Por --delete, respaldar en la carpeta equivocada borraría lo que haya allí. Se confirma cada
// vez que el destino cambia (la primera vez incluida), y queda guardado al aceptarlo.
bool PaginaRespaldo::confirmarDestino()
{
    QSettings guardado = ajustes();
    if (guardado.value(ClaveDestinoConfirmado).toString() == rutaEnServidor())
        return true;
    QMessageBox caja(QMessageBox::Warning, windowTitle(),
                     tr("El respaldo va a ir a:\n%1\n\nLo que haya en esa carpeta y no esté en esta computadora se va a borrar "
                        "(queda %2 días en «versiones anteriores»). ¿Es correcto?")
                         .arg(rutaEnServidor()).arg(respaldo::DiasAnteriores), QMessageBox::NoButton, this);
    const QPushButton *si = caja.addButton(tr("Sí, respaldar"), QMessageBox::AcceptRole);
    caja.setDefaultButton(caja.addButton(tr("No"), QMessageBox::RejectRole));
    caja.exec();
    if (caja.clickedButton() != si)
        return false;
    guardado.setValue(ClaveDestinoConfirmado, rutaEnServidor());
    return true;
}

bool PaginaRespaldo::confirmarRestauracion()
{
    const QString que = relativaRestaurar.isEmpty() ? tr("todo el respaldo")
                                                    : tr("«%1»").arg(QDir::toNativeSeparators(relativaRestaurar));
    const QString desde = ui->comboOrigen->currentData().toString().isEmpty()
                              ? tr("del último respaldo")
                              : tr("de la versión anterior del %1").arg(fechaLegible(ui->comboOrigen->currentData().toString()));
    const QString donde = QDir::toNativeSeparators(QDir::home().filePath(relativaRestaurar));
    const QString como = ui->checkReemplazar->isChecked()
        ? tr("Los archivos que ya están en la computadora se reemplazan por los del respaldo; lo que había queda guardado en %1.")
              .arg(QDir::toNativeSeparators(QDir(Herramientas::datos()).filePath(QStringLiteral("reemplazados"))))
        : tr("Los archivos que ya están en la computadora no se tocan: solo se traen los que faltan.");
    QMessageBox caja(QMessageBox::Question, windowTitle(),
                     tr("Se va a traer %1 %2 a:\n%3\n\n%4\nNo se borra nada de la computadora. ¿Seguir?").arg(que, desde, donde, como),
                     QMessageBox::NoButton, this);
    const QPushButton *si = caja.addButton(tr("Sí, restaurar"), QMessageBox::AcceptRole);
    caja.setDefaultButton(caja.addButton(tr("No"), QMessageBox::RejectRole));
    caja.exec();
    return caja.clickedButton() == si;
}

bool PaginaRespaldo::ocupada() const
{
    return trabajando;
}

void PaginaRespaldo::cancelarYEsperar()
{
    cancelar();
    proceso.waitForFinished(5000);
}

void PaginaRespaldo::habilitarFormulario(bool habilitado)
{
    ui->grupoPersona->setEnabled(habilitado);
    ui->grupoComputadora->setEnabled(habilitado);
    ui->grupoContrasena->setEnabled(habilitado);
    ui->pestanas->setEnabled(habilitado);
    ui->botonCancelar->setEnabled(!habilitado);
    // El botón deshabilitado pierde el foco: así Cancelar sigue a mano con el teclado
    if (habilitado)
        ui->editContrasena->setFocus();
    else
        ui->botonCancelar->setFocus();
    emit ocupacionCambiada();
}

void PaginaRespaldo::anotar(const QString &texto)
{
    ui->registro->appendPlainText(texto);
}

void PaginaRespaldo::elegirParaRestaurar(bool carpeta)
{
    const QString inicio = QDir::home().filePath(respaldo::rutaRelativaSegura(ui->editRestaurar->text()));
    const QString elegido = carpeta ? QFileDialog::getExistingDirectory(this, tr("Carpeta para restaurar"), inicio)
                                    : QFileDialog::getOpenFileName(this, tr("Archivo para restaurar"), inicio);
    if (elegido.isEmpty())
        return;
    const QString relativa = QDir::home().relativeFilePath(elegido);
    if (relativa.startsWith(QStringLiteral(".."))) {
        tema::mostrarEstado(ui->etiquetaEstado, tr("Tiene que estar dentro de tu carpeta personal."), tema::Estado::Aviso);
        return;
    }
    ui->editRestaurar->setText(QDir::toNativeSeparators(relativa == QStringLiteral(".") ? QString() : relativa));
}

// Lo que piden las tres tareas (respaldo, restauración y buscar versiones)
bool PaginaRespaldo::validar()
{
    if (trabajando)
        return false;
    if (persona().isEmpty()) {
        tema::mostrarEstado(ui->etiquetaEstado, tr("Escribí de quién es la computadora: una palabra, con letras, números, punto o guion."));
        ui->editPersona->setFocus();
        return false;
    }
    if (computadora().isEmpty()) {
        tema::mostrarEstado(ui->etiquetaEstado, tr("Escribí un nombre para la computadora, sin barras, sin empezar con un punto "
                                                   "y que no termine en «.anteriores»."));
        ui->editComputadora->setFocus();
        return false;
    }
    if (ui->editContrasena->text().isEmpty()) {
        tema::mostrarEstado(ui->etiquetaEstado, tr("Escribí la contraseña SSH del servidor."));
        ui->editContrasena->setFocus();
        return false;
    }
    return true;
}

void PaginaRespaldo::empezar()
{
    if (!validar())
        return;
    // Con --delete, un origen que se ve vacío borraría todo el respaldo del servidor
    if (QDir::home().entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
        tema::mostrarEstado(ui->etiquetaEstado, tr("No se pudo leer la carpeta personal."), tema::Estado::Error);
        return;
    }
    ui->etiquetaEstado->clear();
    simular = ui->checkSimular->isChecked();
    // Una simulación no toca nada: ni pregunta ni guarda el destino
    if (!simular && !confirmarDestino())
        return;
    fechaRespaldo = respaldo::nombreFecha(QDateTime::currentDateTime());
    respaldados = 0;
    iniciar(Paso::Respaldo);
}

void PaginaRespaldo::restaurar()
{
    if (!validar())
        return;
    bool ok = false;
    relativaRestaurar = respaldo::rutaRelativaSegura(ui->editRestaurar->text(), &ok);
    if (!ok) {
        tema::mostrarEstado(ui->etiquetaEstado, tr("Escribí una carpeta o un archivo de tu carpeta personal (sin «..» ni unidades como C:), "
                                                   "o dejalo vacío para restaurar todo."), tema::Estado::Aviso);
        ui->editRestaurar->setFocus();
        return;
    }
    ui->etiquetaEstado->clear();
    simular = ui->checkSimularRestaurar->isChecked();
    if (!simular && !confirmarRestauracion())
        return;
    const QString fecha = ui->comboOrigen->currentData().toString();
    baseRestaurar = fecha.isEmpty() ? rutaEnServidor() : respaldo::carpetaAnteriores(rutaEnServidor()) + fecha + u'/';
    reemplazados = ui->checkReemplazar->isChecked() && !simular
        ? QDir(Herramientas::datos()).filePath(QStringLiteral("reemplazados/") + respaldo::nombreFecha(QDateTime::currentDateTime()))
        : QString();
    iniciar(Paso::Restaurar);
}

void PaginaRespaldo::buscarVersiones()
{
    if (!validar())
        return;
    ui->etiquetaEstado->clear();
    simular = false;
    iniciar(Paso::Versiones);
}

void PaginaRespaldo::iniciar(Paso primero)
{
    contrasena = ui->editContrasena->text();
    ui->editContrasena->clear();    // no queda escrita en la ventana
    primerPaso = primero;
    cancelado = false;
    pendiente.clear();
    resumen = ResumenSimulacion();
    ui->registro->clear();
    ui->etiquetaArchivo->clear();
    ui->barraProgreso->setValue(0);
    ui->barraProgreso->show();
    tema::mostrarEstado(ui->etiquetaEstado, tr("Buscando el servidor…"));
    trabajando = true;
    habilitarFormulario(false);
    probarServidor(0);
}

// Prueba el puerto de SSH de cada dirección, en orden
void PaginaRespaldo::probarServidor(int indice)
{
    if (cancelado || indice >= servidores().size()) {
        tema::mostrarEstado(ui->etiquetaEstado, cancelado ? tr("Cancelado.")
                                              : tr("No se encontró el servidor (ni en la red de casa ni por Tailscale)."),
                            cancelado ? tema::Estado::Neutro : tema::Estado::Error);
        terminar();
        return;
    }
    prueba = new QTcpSocket(this);
    auto *espera = new QTimer(prueba);
    espera->setSingleShot(true);
    const auto fallo = [this, indice] {
        prueba->disconnect(this);
        prueba->deleteLater();
        prueba = nullptr;
        probarServidor(indice + 1);
    };
    connect(prueba, &QTcpSocket::connected, this, [this, indice] {
        prueba->disconnect(this);
        prueba->abort();
        prueba->deleteLater();
        prueba = nullptr;
        if (cancelado) {
            probarServidor(servidores().size());
            return;
        }
        servidor = servidores().at(indice);
        if (servidor != servidores().constFirst())
            anotar(tr("Conectando por Tailscale (%1).").arg(servidor));
        correr(primerPaso);
    });
    connect(prueba, &QTcpSocket::errorOccurred, this, fallo);
    connect(espera, &QTimer::timeout, this, fallo);
    prueba->connectToHost(servidores().at(indice), respaldo::PuertoSsh);
    espera->start(EsperaServidorMs);
}

QString PaginaRespaldo::comandoSsh() const
{
    // Las huellas vienen compiladas en el programa (ConfigServidor::huellasSsh): desde una red ajena,
    // otro equipo no puede hacerse pasar por el servidor y recibir la contraseña
    const QString rutaHuellas = QDir(Herramientas::datos()).filePath(QStringLiteral("huellas-servidor"));
    QFile archivo(rutaHuellas);
    if (archivo.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        archivo.write(ConfigServidor::huellasSsh({QStringLiteral("servidor-respaldos")}).join(u'\n').toUtf8() + '\n');
    return respaldo::comandoSsh(Herramientas::rutaMsys(Herramientas::programa(QStringLiteral("ssh.exe"))),
                                Herramientas::rutaMsys(rutaHuellas));
}

void PaginaRespaldo::correr(Paso siguiente)
{
    paso = siguiente;
    lector = respaldo::LectorRsync();
    pendiente.clear();
    const QString remoto = QStringLiteral("%1@%2:").arg(usuario(), servidor);
    QStringList argumentos;

    switch (paso) {
    case Paso::Respaldo: {
        anotar(tr("%1 → %2:%3").arg(QDir::toNativeSeparators(QDir::homePath()), servidor, rutaEnServidor()));
        tema::mostrarEstado(ui->etiquetaEstado, simular ? tr("Simulando el respaldo (no cambia nada)…") : tr("Respaldando…"));
        respaldo::OpcionesRespaldo opciones;
        opciones.ssh = comandoSsh();
        opciones.origen = Herramientas::rutaMsys(QDir::homePath()) + u'/';
        opciones.destino = remoto + rutaEnServidor();
        opciones.excluidos = respaldo::exclusiones(extras());
        opciones.simular = simular;
        if (!simular)
            opciones.backupDir = respaldo::backupDirRelativo(computadora(), fechaRespaldo);
        argumentos = respaldo::argumentosRespaldo(opciones);
        break;
    }
    case Paso::Listar:
    case Paso::Versiones:
        if (paso == Paso::Versiones) {
            tema::mostrarEstado(ui->etiquetaEstado, tr("Buscando versiones anteriores en el servidor…"));
            ui->barraProgreso->setRange(0, 0);
        }
        argumentos = respaldo::argumentosListar(comandoSsh(), remoto + respaldo::carpetaAnteriores(rutaEnServidor()));
        break;
    case Paso::Limpiar:
        vacia = std::make_unique<QTemporaryDir>(QDir(Herramientas::temporal()).filePath(QStringLiteral("vacia-XXXXXX")));
        if (!vacia->isValid() || !QDir(vacia->path()).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
            anotar(tr("No se pudieron borrar las versiones anteriores viejas (no hay una carpeta vacía a mano)."));
            terminar();
            return;
        }
        argumentos = respaldo::argumentosLimpiar(comandoSsh(), Herramientas::rutaMsys(vacia->path()),
                                                 remoto + respaldo::carpetaAnteriores(rutaEnServidor()), viejas);
        break;
    case Paso::Restaurar: {
        anotar(tr("%1:%2%3 → %4").arg(servidor, baseRestaurar, relativaRestaurar,
                                      QDir::toNativeSeparators(QDir::home().filePath(relativaRestaurar))));
        tema::mostrarEstado(ui->etiquetaEstado, simular ? tr("Simulando la restauración (no trae nada)…") : tr("Restaurando…"));
        respaldo::OpcionesRestaurar opciones;
        opciones.ssh = comandoSsh();
        opciones.servidor = QStringLiteral("%1@%2").arg(usuario(), servidor);
        opciones.base = baseRestaurar;
        opciones.relativa = relativaRestaurar;
        opciones.carpetaPersonal = Herramientas::rutaMsys(QDir::homePath());
        opciones.reemplazar = ui->checkReemplazar->isChecked();
        opciones.reemplazados = reemplazados.isEmpty() ? QString() : Herramientas::rutaMsys(reemplazados);
        opciones.simular = simular;
        argumentos = respaldo::argumentosRestaurar(opciones);
        break;
    }
    case Paso::Ninguno:
        return;
    }

    // ssh le pide la contraseña a askpass, y askpass la toma de esta variable: no aparece en la
    // lista de procesos ni queda guardada
    QProcessEnvironment variables = Herramientas::ambiente();
    variables.insert(QStringLiteral("SSH_ASKPASS"), Herramientas::rutaMsys(Herramientas::programa(QStringLiteral("askpass.exe"))));
    variables.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force"));
    variables.insert(QStringLiteral("WM_CONTRASENA"), contrasena);
    proceso.setProcessEnvironment(variables);
    proceso.start(Herramientas::programa(QStringLiteral("rsync.exe")), argumentos);
    proceso.setProcessEnvironment(Herramientas::ambiente());    // no queda guardada
}

void PaginaRespaldo::terminar()
{
    contrasena.clear();
    vacia.reset();
    paso = Paso::Ninguno;
    ui->barraProgreso->setRange(0, 100);
    ui->etiquetaArchivo->clear();
    trabajando = false;
    habilitarFormulario(true);
}

void PaginaRespaldo::cancelar()
{
    if (!trabajando)
        return;
    cancelado = true;
    tema::mostrarEstado(ui->etiquetaEstado, tr("Cancelando…"));
    Herramientas::cortar(proceso);
}

// rsync separa las actualizaciones de progreso con \r y el resto con \n
void PaginaRespaldo::leerSalida()
{
    pendiente += proceso.readAll();
    qsizetype inicio = 0;
    for (qsizetype i = 0; i < pendiente.size(); ++i) {
        if (pendiente.at(i) == '\r' || pendiente.at(i) == '\n') {
            procesarLinea(QString::fromUtf8(pendiente.mid(inicio, i - inicio)));
            inicio = i + 1;
        }
    }
    pendiente.remove(0, inicio);
}

void PaginaRespaldo::procesarLinea(const QString &texto)
{
    const QString linea = texto.trimmed();
    if (linea.isEmpty())
        return;
    const bool copia = paso == Paso::Respaldo || paso == Paso::Restaurar;
    if (copia && simular && resumen.procesar(linea))
        return;

    const respaldo::LectorRsync::Linea leida = lector.procesar(linea);
    switch (leida.tipo) {
    case respaldo::LectorRsync::Tipo::Progreso:
        if (copia)
            ui->barraProgreso->setValue(leida.porcentaje);
        break;
    case respaldo::LectorRsync::Tipo::Aviso:
        // Buscar versiones: que todavía no exista la carpeta no es un aviso
        if (paso != Paso::Listar && paso != Paso::Versiones)
            anotar(linea);
        break;
    case respaldo::LectorRsync::Tipo::Archivo:
        // En la simulación el resto son las estadísticas de rsync, que no son nombres de archivo
        if (copia && !simular) {
            // El nombre se recorta por el medio para que entre en una línea
            const QFontMetrics medidas(ui->etiquetaArchivo->font());
            ui->etiquetaArchivo->setText(medidas.elidedText(linea, Qt::ElideMiddle, ui->etiquetaArchivo->width()));
        }
        break;
    case respaldo::LectorRsync::Tipo::Respaldado:
    case respaldo::LectorRsync::Tipo::Nada:
        break;
    }
}

// Los errores que valen igual para las tres tareas. Devuelve verdadero si mostró uno.
bool PaginaRespaldo::mostrarError(Final final, int codigo, const QString &que)
{
    switch (final) {
    case Final::Cancelado:
        tema::mostrarEstado(ui->etiquetaEstado, tr("%1: cancelado.").arg(que));
        return true;
    case Final::Fallo:
        tema::mostrarEstado(ui->etiquetaEstado, tr("%1: falló (rsync se cerró de golpe). Volvé a intentarlo.").arg(que), tema::Estado::Error);
        return true;
    case Final::HuellaDistinta:
        ui->barraProgreso->hide();
        tema::mostrarEstado(ui->etiquetaEstado, tr("Del otro lado contestó un equipo que no es el servidor (la huella no coincide). "
                                       "No se mandó la contraseña. Si el servidor se reinstaló, hace falta una versión nueva del programa."),
                            tema::Estado::Error);
        return true;
    case Final::ContrasenaMala:
        ui->barraProgreso->hide();
        tema::mostrarEstado(ui->etiquetaEstado, tr("Contraseña incorrecta, volvé a escribirla."), tema::Estado::Error);
        return true;
    case Final::Error:
        tema::mostrarEstado(ui->etiquetaEstado, tr("%1: rsync terminó con errores (código %2). Revisá la conexión con el servidor y volvé a intentarlo.")
                                                    .arg(que).arg(codigo), tema::Estado::Error);
        return true;
    case Final::Ok:
    case Final::Incompleto:
        return false;
    }
    return false;
}

void PaginaRespaldo::terminoProceso(int codigo, QProcess::ExitStatus estado)
{
    leerSalida();
    if (!pendiente.isEmpty()) {
        procesarLinea(QString::fromUtf8(pendiente));
        pendiente.clear();
    }
    ui->etiquetaArchivo->clear();
    const Final final = respaldo::clasificar(codigo, estado, cancelado, lector);
    switch (paso) {
    case Paso::Respaldo:
        terminoRespaldo(final, codigo);
        break;
    case Paso::Restaurar:
        terminoRestaurar(final, codigo);
        break;
    case Paso::Versiones:
        terminoVersiones(final, codigo);
        break;
    case Paso::Listar: {
        // El respaldo ya terminó bien: si esto falla, solo no se limpian las versiones viejas
        viejas = final == Final::Ok ? respaldo::anterioresViejas(respaldo::fechasDeListado(lector.listado().join(u'\n')),
                                                                  QDateTime::currentDateTime())
                                    : QStringList();
        if (viejas.isEmpty() || cancelado)
            terminar();
        else
            correr(Paso::Limpiar);
        break;
    }
    case Paso::Limpiar:
        if (final == Final::Ok)
            anotar(tr("Se borraron %n versión(es) anterior(es) de más de %1 días.", nullptr, int(viejas.size())).arg(respaldo::DiasAnteriores));
        else if (!cancelado)
            anotar(tr("No se pudieron borrar las versiones anteriores de más de %1 días (código %2).").arg(respaldo::DiasAnteriores).arg(codigo));
        terminar();
        break;
    case Paso::Ninguno:
        terminar();
        break;
    }
}

void PaginaRespaldo::terminoRespaldo(Final final, int codigo)
{
    if (mostrarError(final, codigo, tr("Respaldo"))) {
        terminar();
        return;
    }
    ui->barraProgreso->setValue(100);
    if (final == Final::Ok) {
        tema::mostrarEstado(ui->etiquetaEstado, simular ? tr("Simulación completada: no se cambió nada.")
                                                        : tr("Respaldo completado."), tema::Estado::Ok);
    } else {
        // Archivos que no se pudieron leer o que desaparecieron durante la copia
        tema::mostrarEstado(ui->etiquetaEstado, simular
            ? tr("Simulación completada, pero algunos archivos no se podrían copiar (ver los avisos).")
            : tr("Terminó, pero algunos archivos no se pudieron copiar (ver los avisos)."), tema::Estado::Aviso);
    }
    if (simular) {
        mostrarResumen(false);
        terminar();
        return;
    }
    ajustes().setValue(ClaveUltimo, QDateTime::currentDateTime().toString(Qt::ISODate));
    mostrarUltimo();
    respaldados = lector.respaldados();
    anotar({});
    if (respaldados == 0)
        anotar(tr("No se borró ni se reemplazó nada del respaldo: no hizo falta guardar versiones anteriores."));
    else
        anotar(tr("%n archivo(s) borrado(s) o cambiado(s) fueron a «anteriores» (%1).", nullptr, respaldados)
                   .arg(respaldo::carpetaAnteriores(rutaEnServidor()) + fechaRespaldo));
    // Después, se borran las versiones anteriores de más de 30 días (otra conexión, con la misma contraseña)
    correr(Paso::Listar);
}

void PaginaRespaldo::terminoRestaurar(Final final, int codigo)
{
    // Lo pedido no está en el servidor: rsync avisa que no encuentra el origen (y termina con 23)
    const QString salida = lector.listado().join(u'\n');
    if ((final == Final::Incompleto || final == Final::Error) && salida.contains(QStringLiteral("No such file or directory"))
        && (salida.contains(QStringLiteral("link_stat")) || salida.contains(QStringLiteral("change_dir")))) {
        tema::mostrarEstado(ui->etiquetaEstado, tr("Eso no está en el respaldo del servidor (revisá el nombre o elegí otra fecha)."),
                            tema::Estado::Error);
        terminar();
        return;
    }
    if (mostrarError(final, codigo, tr("Restauración"))) {
        terminar();
        return;
    }
    ui->barraProgreso->setValue(100);
    if (simular) {
        tema::mostrarEstado(ui->etiquetaEstado, final == Final::Ok ? tr("Simulación completada: no se trajo nada.")
                                                                   : tr("Simulación completada, con avisos (ver abajo)."),
                            final == Final::Ok ? tema::Estado::Ok : tema::Estado::Aviso);
        mostrarResumen(true);
    } else if (final == Final::Ok) {
        tema::mostrarEstado(ui->etiquetaEstado, tr("Restauración completada."), tema::Estado::Ok);
    } else {
        tema::mostrarEstado(ui->etiquetaEstado, tr("Restauración terminada, pero algunos archivos no se pudieron traer (ver los avisos)."),
                            tema::Estado::Aviso);
    }
    if (!reemplazados.isEmpty() && QFileInfo(reemplazados).isDir())
        anotar(tr("Lo que había antes en la computadora quedó en %1.").arg(QDir::toNativeSeparators(reemplazados)));
    terminar();
}

void PaginaRespaldo::terminoVersiones(Final final, int codigo)
{
    // Si la carpeta de versiones anteriores todavía no existe, rsync termina con 23
    const bool noHay = final == Final::Incompleto || (final == Final::Ok && respaldo::fechasDeListado(lector.listado().join(u'\n')).isEmpty());
    if (!noHay && mostrarError(final, codigo, tr("Buscar versiones anteriores"))) {
        terminar();
        return;
    }
    while (ui->comboOrigen->count() > 1)
        ui->comboOrigen->removeItem(1);
    const QStringList fechas = respaldo::fechasDeListado(lector.listado().join(u'\n'));
    for (const QString &fecha : fechas)
        ui->comboOrigen->addItem(tr("Versión anterior del %1").arg(fechaLegible(fecha)), fecha);
    ui->barraProgreso->hide();
    if (fechas.isEmpty()) {
        tema::mostrarEstado(ui->etiquetaEstado, tr("Todavía no hay versiones anteriores de esta computadora en el servidor."));
    } else {
        tema::mostrarEstado(ui->etiquetaEstado, tr("Hay %n versión(es) anterior(es). Cada una tiene solo lo que se borró o cambió en ese respaldo.",
                                                   nullptr, int(fechas.size())), tema::Estado::Ok);
        ui->comboOrigen->setCurrentIndex(1);
    }
    terminar();
}

void PaginaRespaldo::mostrarResumen(bool restauracion)
{
    anotar({});
    const QString tamano = resumen.tamanoACopiar().isEmpty() ? QString() : tr(" (%1).").arg(resumen.tamanoACopiar());
    if (restauracion) {
        anotar(tr("Se traerían %n archivo(s)", nullptr, int(resumen.archivosACopiar())) + tamano);
        return;
    }
    anotar(tr("Se copiarían %n archivo(s)", nullptr, int(resumen.archivosACopiar())) + tamano);

    if (resumen.borradosOmitidos()) {
        // Con errores de lectura, rsync no borra nada: la lista de abajo no sería la de una copia sin errores
        anotar(tr("No se sabe qué se borraría: por los errores de lectura, rsync no evaluó los borrados."));
    } else if (resumen.archivosABorrar() == 0) {
        anotar(tr("No se borraría ningún archivo del respaldo."));
    } else {
        anotar(tr("Se borrarían %n archivo(s) del respaldo (quedarían en «anteriores»):", nullptr, int(resumen.archivosABorrar())));
        const QStringList lista = resumen.primerosABorrar();
        for (const QString &ruta : lista)
            anotar(QStringLiteral("  ") + ruta);
        if (resumen.archivosABorrar() > lista.size())
            anotar(tr("  …y %1 más.").arg(resumen.archivosABorrar() - lista.size()));
    }
}
