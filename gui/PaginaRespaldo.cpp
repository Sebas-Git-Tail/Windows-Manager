#include "PaginaRespaldo.h"
#include "ui_PaginaRespaldo.h"

#include "Herramientas.h"

#include <QDir>
#include <QFontMetrics>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSysInfo>
#include <QSettings>
#include <QTcpSocket>
#include <QTimer>

namespace {

// Los mismos datos que Common/Servidor.sh. Primero la red de casa y, si no contesta, Tailscale.
const QStringList Servidores{QStringLiteral("192.168.100.14"), QStringLiteral("100.101.213.44")};
const QString Usuario = QStringLiteral("sebastian");
const QString CarpetaRespaldos = QStringLiteral("/home/sebastian/Respaldos/");
const QString ClaveNombre = QStringLiteral("respaldo/nombre");
const QString ClaveComputadora = QStringLiteral("respaldo/computadora");
const QString ClaveDestinoConfirmado = QStringLiteral("respaldo/destinoConfirmado");
constexpr int CodigoIncompleto = 23;    // rsync: archivos que no se pudieron leer
constexpr int CodigoDesaparecido = 24;  // rsync: archivos que se borraron durante la copia
constexpr int EsperaServidorMs = 2000;

QString archivoAjustes()
{
    return QDir(Herramientas::datos()).filePath(QStringLiteral("Windows-Manager.ini"));
}

// Lo que no se copia (rutas desde la carpeta personal). AppData es muy pesada y sus programas la
// tienen abierta; NTUSER es el registro, que Windows tiene bloqueado; OneDrive traería todo lo
// que esté solo en la nube. El resto es lo mismo que excluye Rsync_Filtro.conf en Linux.
const QStringList Excluidos{
    QStringLiteral("/AppData/"), QStringLiteral("/NTUSER.DAT*"), QStringLiteral("/ntuser.*"), QStringLiteral("/OneDrive*/"),
    QStringLiteral("/.ssh/"), QStringLiteral("/.cache/"), QStringLiteral("/.java/"), QStringLiteral("/Games/"),
    QStringLiteral("/VirtualBox VMs/"), QStringLiteral("/.npm/"), QStringLiteral("/.ollama/")};

} // namespace

PaginaRespaldo::PaginaRespaldo(QWidget *parent)
    : QWidget(parent), ui(new Ui::PaginaRespaldo)
{
    ui->setupUi(this);
    setWindowTitle(tr("Respaldo"));
    ui->barraProgreso->hide();

    const QSettings ajustes(archivoAjustes(), QSettings::IniFormat);
    ui->editPersona->setText(ajustes.value(ClaveNombre).toString());
    // Se propone el nombre del equipo, pero una vez guardado es fijo: si cambiara solo cuando
    // alguien renombra la computadora, el respaldo empezaría en otra carpeta
    ui->editComputadora->setText(ajustes.value(ClaveComputadora, QSysInfo::machineHostName()).toString());
    const auto alEditar = [this] {
        mostrarDescripcion();
        guardarNombres();
    };
    connect(ui->editPersona, &QLineEdit::textChanged, this, alEditar);
    connect(ui->editComputadora, &QLineEdit::textChanged, this, alEditar);

    connect(ui->botonEmpezar, &QPushButton::clicked, this, &PaginaRespaldo::empezar);
    connect(ui->editContrasena, &QLineEdit::returnPressed, this, &PaginaRespaldo::empezar);
    connect(ui->botonCancelar, &QPushButton::clicked, this, &PaginaRespaldo::cancelar);

    proceso.setProcessChannelMode(QProcess::MergedChannels);
    connect(&proceso, &QProcess::readyReadStandardOutput, this, &PaginaRespaldo::leerSalida);
    connect(&proceso, &QProcess::finished, this, &PaginaRespaldo::terminoProceso);
    connect(&proceso, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        ui->etiquetaEstado->setText(tr("× No se pudo ejecutar rsync: %1").arg(proceso.errorString()));
        trabajando = false;
        habilitarFormulario(true);
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
    const QString aviso = tr("Sin AppData, OneDrive ni el registro de Windows. Lo que borraste de la computadora también se borra del respaldo.");
    ui->etiquetaDescripcion->setText(persona().isEmpty() || computadora().isEmpty()
        ? tr("Copia tu carpeta personal al servidor, a la carpeta de la persona. %1").arg(aviso)
        : tr("Copia tu carpeta personal al servidor, a %1. %2").arg(rutaEnServidor(), aviso));
}

// El nombre es una carpeta del servidor: nada de barras ni «..», que la sacarían de Respaldos
QString PaginaRespaldo::persona() const
{
    static const QRegularExpression valida(QStringLiteral("^[a-z0-9][a-z0-9._-]*$"));
    const QString nombre = ui->editPersona->text().trimmed().toLower();    // las carpetas van en minúsculas
    return valida.match(nombre).hasMatch() ? nombre : QString();
}

// Admite espacios y mayúsculas, pero no puede salirse de la carpeta de la persona
QString PaginaRespaldo::computadora() const
{
    static const QRegularExpression valido(QStringLiteral("^[^/\\\\.\\p{Cc}][^/\\\\\\p{Cc}]*$"));
    const QString nombre = ui->editComputadora->text().trimmed();
    return valido.match(nombre).hasMatch() ? nombre : QString();
}

// La carpeta de destino lleva la persona y el nombre de la computadora
QString PaginaRespaldo::rutaEnServidor() const
{
    return CarpetaRespaldos + persona() + u'/' + computadora() + u'/';
}

// Se guardan apenas se escriben, para que sigan ahí al volver a abrir la app. Haberlos escrito
// no confirma el destino: eso lo hace confirmarDestino() antes de la primera copia real.
void PaginaRespaldo::guardarNombres()
{
    QSettings ajustes(archivoAjustes(), QSettings::IniFormat);
    if (persona().isEmpty())
        ajustes.remove(ClaveNombre);
    else
        ajustes.setValue(ClaveNombre, persona());
    if (computadora().isEmpty())
        ajustes.remove(ClaveComputadora);
    else
        ajustes.setValue(ClaveComputadora, computadora());
}

// Por --delete, respaldar en la carpeta equivocada borraría lo que haya allí. Se confirma cada
// vez que el destino cambia (la primera vez incluida), y queda guardado al aceptarlo.
bool PaginaRespaldo::confirmarDestino()
{
    QSettings ajustes(archivoAjustes(), QSettings::IniFormat);
    if (ajustes.value(ClaveDestinoConfirmado).toString() == rutaEnServidor())
        return true;
    QMessageBox caja(QMessageBox::Warning, windowTitle(),
                     tr("El respaldo va a ir a:\n%1\n\nLo que haya en esa carpeta y no esté en esta computadora se va a borrar. ¿Es correcto?")
                         .arg(rutaEnServidor()), QMessageBox::NoButton, this);
    const QPushButton *si = caja.addButton(tr("Sí, respaldar"), QMessageBox::AcceptRole);
    caja.setDefaultButton(caja.addButton(tr("No"), QMessageBox::RejectRole));
    caja.exec();
    if (caja.clickedButton() != si)
        return false;
    ajustes.setValue(ClaveDestinoConfirmado, rutaEnServidor());
    return true;
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
    ui->checkSimular->setEnabled(habilitado);
    ui->botonEmpezar->setEnabled(habilitado);
    ui->botonCancelar->setEnabled(!habilitado);
    // El botón deshabilitado pierde el foco: así Cancelar sigue a mano con el teclado
    (habilitado ? ui->botonEmpezar : ui->botonCancelar)->setFocus();
    emit ocupacionCambiada();
}

void PaginaRespaldo::anotar(const QString &texto)
{
    ui->registro->appendPlainText(texto);
}

void PaginaRespaldo::empezar()
{
    if (trabajando)
        return;
    if (persona().isEmpty()) {
        ui->etiquetaEstado->setText(tr("Escribí de quién es la computadora: una palabra, con letras, números, punto o guion."));
        ui->editPersona->setFocus();
        return;
    }
    if (computadora().isEmpty()) {
        ui->etiquetaEstado->setText(tr("Escribí un nombre para la computadora, sin barras y sin empezar con un punto."));
        ui->editComputadora->setFocus();
        return;
    }
    contrasena = ui->editContrasena->text();
    if (contrasena.isEmpty()) {
        ui->etiquetaEstado->setText(tr("Escribí la contraseña SSH del servidor."));
        ui->editContrasena->setFocus();
        return;
    }
    // Con --delete, un origen que se ve vacío borraría todo el respaldo del servidor
    if (QDir::home().entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
        ui->etiquetaEstado->setText(tr("× No se pudo leer la carpeta personal."));
        return;
    }
    ui->etiquetaEstado->clear();
    // Una simulación no toca nada: ni pregunta ni guarda el destino
    if (!ui->checkSimular->isChecked() && !confirmarDestino()) {
        contrasena.clear();
        return;
    }
    ui->editContrasena->clear();    // no queda escrita en la ventana

    cancelado = false;
    errorConexion.clear();
    sinDatos = false;
    pendiente.clear();
    resumen = ResumenSimulacion();
    ui->registro->clear();
    ui->etiquetaArchivo->clear();
    ui->barraProgreso->setValue(0);
    ui->barraProgreso->show();
    ui->etiquetaEstado->setText(tr("Buscando el servidor…"));
    trabajando = true;
    habilitarFormulario(false);
    probarServidor(0);
}

// Prueba el puerto de SSH de cada dirección, en orden
void PaginaRespaldo::probarServidor(int indice)
{
    if (cancelado || indice >= Servidores.size()) {
        ui->etiquetaEstado->setText(cancelado ? tr("Respaldo cancelado.")
                                              : tr("× No se encontró el servidor (ni en la red de casa ni por Tailscale)."));
        contrasena.clear();
        ui->barraProgreso->hide();
        trabajando = false;
        habilitarFormulario(true);
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
        if (cancelado)
            probarServidor(Servidores.size());
        else
            copiar(Servidores.at(indice));
    });
    connect(prueba, &QTcpSocket::errorOccurred, this, fallo);
    connect(espera, &QTimer::timeout, this, fallo);
    prueba->connectToHost(Servidores.at(indice), 22);
    espera->start(EsperaServidorMs);
}

void PaginaRespaldo::copiar(const QString &servidor)
{
    if (servidor != Servidores.constFirst())
        anotar(tr("Conectando por Tailscale (%1).").arg(servidor));
    const bool simular = ui->checkSimular->isChecked();
    const QString origen = Herramientas::rutaMsys(QDir::homePath()) + u'/';
    anotar(tr("%1 → %2:%3").arg(QDir::toNativeSeparators(QDir::homePath()), servidor, rutaEnServidor()));
    ui->etiquetaEstado->setText(simular ? tr("Simulando el respaldo (no cambia nada)…") : tr("Respaldando…"));

    // accept-new: la primera vez acepta la huella del servidor y después la exige igual.
    // Una sola contraseña por intento: si está mal, no se insiste contra el servidor.
    const QString huellas = Herramientas::rutaMsys(QDir(Herramientas::datos()).filePath(QStringLiteral("known_hosts")));
    const QString ssh = QStringLiteral("\"%1\" -F none -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=\"%2\" "
                                       "-o PubkeyAuthentication=no -o PreferredAuthentications=password,keyboard-interactive "
                                       "-o NumberOfPasswordPrompts=1 -o ConnectTimeout=10")
                            .arg(Herramientas::rutaMsys(Herramientas::programa(QStringLiteral("ssh.exe"))), huellas);

    // Sin -a: los permisos de Windows no significan nada en el servidor. Por eso -r -t y
    // --chmod; sin -l, porque las carpetas de sistema de Windows son enlaces que dan vueltas
    QStringList argumentos{QStringLiteral("-rth"), QStringLiteral("--chmod=D755,F644"),
                           QStringLiteral("--info=progress2,name1"),
                           // sin esto, rsync escribe las tildes de los nombres como códigos
                           QStringLiteral("-8"),
                           QStringLiteral("-e"), ssh,
                           // la ruta puede tener espacios (el nombre de la computadora)
                           QStringLiteral("--protect-args"),
                           // la carpeta de la persona puede no existir todavía
                           QStringLiteral("--mkpath"),
                           QStringLiteral("--delete")};
    for (const QString &excluido : Excluidos)
        argumentos << QStringLiteral("--exclude") << excluido;
    if (simular) {
        argumentos << QStringLiteral("--dry-run") << QStringLiteral("--itemize-changes") << QStringLiteral("--stats")
                   << QStringLiteral("--info=del1");
    }
    argumentos << origen << QStringLiteral("%1@%2:%3").arg(Usuario, servidor, rutaEnServidor());

    // ssh le pide la contraseña a askpass, y askpass la toma de esta variable: no aparece en la
    // lista de procesos ni queda guardada
    QProcessEnvironment variables = Herramientas::ambiente();
    variables.insert(QStringLiteral("SSH_ASKPASS"), Herramientas::rutaMsys(Herramientas::programa(QStringLiteral("askpass.exe"))));
    variables.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force"));
    variables.insert(QStringLiteral("WM_CONTRASENA"), contrasena);
    contrasena.clear();
    proceso.setProcessEnvironment(variables);
    proceso.start(Herramientas::programa(QStringLiteral("rsync.exe")), argumentos);
    proceso.setProcessEnvironment(Herramientas::ambiente());    // no queda guardada
}

void PaginaRespaldo::cancelar()
{
    if (!trabajando)
        return;
    cancelado = true;
    ui->etiquetaEstado->setText(tr("Cancelando…"));
    Herramientas::cortar(proceso);
}

// rsync separa las actualizaciones de progreso con \r y el resto con \n
void PaginaRespaldo::leerSalida()
{
    pendiente += proceso.readAll();
    qsizetype inicio = 0;
    for (qsizetype i = 0; i < pendiente.size(); ++i) {
        if (pendiente.at(i) == '\r' || pendiente.at(i) == '\n') {
            procesarLinea(QString::fromUtf8(pendiente.mid(inicio, i - inicio)).trimmed());
            inicio = i + 1;
        }
    }
    pendiente.remove(0, inicio);
}

void PaginaRespaldo::procesarLinea(const QString &linea)
{
    // «2.00MB  99%  2.00GB/s ...»: anclada, porque un archivo puede llamarse «99% seguro.txt»
    static const QRegularExpression progreso(QStringLiteral("^\\S+\\s+(\\d+)%\\s"));
    if (linea.isEmpty())
        return;
    if (ui->checkSimular->isChecked() && resumen.procesar(linea))
        return;

    const QRegularExpressionMatch coincidencia = progreso.match(linea);
    if (coincidencia.hasMatch()) {
        ui->barraProgreso->setValue(coincidencia.captured(1).toInt());
    } else if (linea.contains(QStringLiteral("Permission denied")) || linea.startsWith(QStringLiteral("ssh:"))
               || linea.contains(QStringLiteral("Connection closed")) || linea.contains(QStringLiteral("Host key verification"))) {
        errorConexion = linea;
        anotar(linea);
    } else if (linea.startsWith(QStringLiteral("IO error"))) {
        anotar(linea);
    } else if (linea.startsWith(QStringLiteral("rsync"))) {
        if (linea.contains(QStringLiteral("(0 bytes received so far)")))
            sinDatos = true;
        anotar(linea);
    } else if (ui->checkSimular->isChecked()) {
        // El resto son las estadísticas de rsync, que no son nombres de archivo
    } else if (linea != QStringLiteral("sending incremental file list") && !linea.startsWith(QStringLiteral("sent "))
               && !linea.startsWith(QStringLiteral("total size")) && linea != QStringLiteral("./")) {
        // El nombre se recorta por el medio para que entre en una línea
        const QFontMetrics medidas(ui->etiquetaArchivo->font());
        ui->etiquetaArchivo->setText(medidas.elidedText(linea, Qt::ElideMiddle, ui->etiquetaArchivo->width()));
    }
}

void PaginaRespaldo::terminoProceso(int codigo, QProcess::ExitStatus estado)
{
    leerSalida();
    trabajando = false;
    habilitarFormulario(true);
    ui->etiquetaArchivo->clear();

    const bool contrasenaMala = sinDatos && errorConexion.contains(QStringLiteral("Permission denied"));
    if (cancelado || estado == QProcess::CrashExit) {
        ui->etiquetaEstado->setText(tr("Respaldo cancelado."));
    } else if (codigo == 0) {
        ui->barraProgreso->setValue(100);
        ui->etiquetaEstado->setText(ui->checkSimular->isChecked() ? tr("✓ Simulación completada: no se cambió nada.")
                                                                  : tr("✓ Respaldo completado."));
        if (ui->checkSimular->isChecked())
            mostrarResumen();
    } else if (codigo == CodigoIncompleto || codigo == CodigoDesaparecido) {
        // Archivos que no se pudieron leer o que desaparecieron durante la copia
        ui->etiquetaEstado->setText(ui->checkSimular->isChecked()
            ? tr("Simulación completada, pero algunos archivos no se podrían copiar (ver los avisos).")
            : tr("Terminó, pero algunos archivos no se pudieron copiar (ver los avisos)."));
        if (ui->checkSimular->isChecked())
            mostrarResumen();
    } else if (contrasenaMala) {
        ui->barraProgreso->hide();
        ui->etiquetaEstado->setText(tr("× Contraseña incorrecta, volvé a escribirla."));
        ui->editContrasena->setFocus();
    } else {
        ui->etiquetaEstado->setText(tr("× rsync terminó con errores (código %1). Revisá la conexión con el servidor y volvé a intentarlo.").arg(codigo));
    }
}

void PaginaRespaldo::mostrarResumen()
{
    anotar({});
    anotar(tr("Se copiarían %n archivo(s)", nullptr, int(resumen.archivosACopiar()))
           + (resumen.tamanoACopiar().isEmpty() ? QString() : tr(" (%1).").arg(resumen.tamanoACopiar())));

    if (resumen.borradosOmitidos()) {
        // Con errores de lectura, rsync no borra nada: la lista de abajo no sería la de una copia sin errores
        anotar(tr("No se sabe qué se borraría: por los errores de lectura, rsync no evaluó los borrados."));
    } else if (resumen.archivosABorrar() == 0) {
        anotar(tr("No se borraría ningún archivo del respaldo."));
    } else {
        anotar(tr("Se borrarían %n archivo(s) del respaldo:", nullptr, int(resumen.archivosABorrar())));
        const QStringList lista = resumen.primerosABorrar();
        for (const QString &ruta : lista)
            anotar(QStringLiteral("  ") + ruta);
        if (resumen.archivosABorrar() > lista.size())
            anotar(tr("  …y %1 más.").arg(resumen.archivosABorrar() - lista.size()));
    }
}
