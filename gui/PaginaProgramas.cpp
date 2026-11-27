#include "PaginaProgramas.h"
#include "Tema.h"
#include "ui_PaginaProgramas.h"

#include "Herramientas.h"
#include "Winget.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>

namespace {

constexpr int RolId = Qt::UserRole;

// winget es un «alias de ejecución» en %LOCALAPPDATA%\Microsoft\WindowsApps (lo instala «Instalador de aplicación»)
QString buscarWinget()
{
    const QString enPath = QStandardPaths::findExecutable(QStringLiteral("winget"));
    if (!enPath.isEmpty())
        return enPath;
    const QString alias = QDir(qEnvironmentVariable("LOCALAPPDATA")).filePath(QStringLiteral("Microsoft/WindowsApps/winget.exe"));
    return QFileInfo::exists(alias) ? alias : QString();
}

} // namespace

PaginaProgramas::PaginaProgramas(QWidget *parent)
    : QWidget(parent), ui(new Ui::PaginaProgramas)
{
    ui->setupUi(this);
    setWindowTitle(tr("Programas"));
    ui->botonBuscar->setIcon(tema::icono(QStringLiteral("buscar")));
    ui->botonActualizables->setIcon(tema::icono(QStringLiteral("actualizar")));
    ui->botonInstalar->setIcon(tema::icono(QStringLiteral("descargar")));
    ui->botonActualizar->setIcon(tema::icono(QStringLiteral("actualizar")));
    ui->botonActualizarTodo->setIcon(tema::icono(QStringLiteral("actualizar")));
    ui->arbolPaquetes->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->arbolPaquetes->header()->setStretchLastSection(false);
    ui->etiquetaFalta->setText(
        tr("<b>Falta winget</b>, el instalador de programas de Windows. Viene con «Instalador de aplicación» "
           "(App Installer) de Microsoft Store: tocá el botón, instalalo o actualizalo, y después tocá «Ya lo instalé». "
           "En Windows 10 muy viejos, primero hay que actualizar Windows."));
    ui->grupoFalta->hide();

    connect(ui->botonBuscar, &QPushButton::clicked, this, [this] {
        const QString texto = ui->editBuscar->text().trimmed();
        if (texto.isEmpty()) {
            tema::mostrarEstado(ui->etiquetaEstado, tr("Escribí el nombre del programa que buscás."), tema::Estado::Aviso);
            ui->editBuscar->setFocus();
            return;
        }
        correr(Tarea::Buscar, winget::argumentosBuscar(texto), tr("Buscando «%1»…").arg(texto));
    });
    connect(ui->editBuscar, &QLineEdit::returnPressed, ui->botonBuscar, &QPushButton::click);
    connect(ui->botonActualizables, &QPushButton::clicked, this, [this] {
        correr(Tarea::Actualizables, winget::argumentosActualizables(), tr("Buscando programas con versiones nuevas…"));
    });
    connect(ui->botonInstalar, &QPushButton::clicked, this, [this] {
        const QTreeWidgetItem *item = ui->arbolPaquetes->currentItem();
        if (item)
            correr(Tarea::Instalar, winget::argumentosInstalar(idElegido()), tr("Instalando %1…").arg(item->text(0)));
    });
    connect(ui->botonActualizar, &QPushButton::clicked, this, [this] {
        const QTreeWidgetItem *item = ui->arbolPaquetes->currentItem();
        if (item)
            correr(Tarea::Actualizar, winget::argumentosActualizar(idElegido()), tr("Actualizando %1…").arg(item->text(0)));
    });
    connect(ui->botonActualizarTodo, &QPushButton::clicked, this, [this] {
        QMessageBox caja(QMessageBox::Question, windowTitle(),
                         tr("¿Actualizar los %n programa(s) de la lista? Puede tardar y algunos piden permiso de administrador.",
                            nullptr, ui->arbolPaquetes->topLevelItemCount()),
                         QMessageBox::NoButton, this);
        const QPushButton *si = caja.addButton(tr("Actualizar todo"), QMessageBox::AcceptRole);
        caja.setDefaultButton(caja.addButton(tr("No"), QMessageBox::RejectRole));
        caja.exec();
        if (caja.clickedButton() == si)
            correr(Tarea::Actualizar, winget::argumentosActualizar({}), tr("Actualizando todos los programas…"));
    });
    connect(ui->botonCancelar, &QPushButton::clicked, this, [this] {
        if (!ocupada())
            return;
        cancelado = true;
        tema::mostrarEstado(ui->etiquetaEstado, tr("Cancelando…"));
        Herramientas::cortar(proceso);
    });
    connect(ui->arbolPaquetes, &QTreeWidget::itemSelectionChanged, this, &PaginaProgramas::actualizarBotones);
    connect(ui->botonTienda, &QPushButton::clicked, this, [] {
        if (!QDesktopServices::openUrl(QUrl(QStringLiteral("ms-windows-store://pdp/?ProductId=9NBLGGH4NNS1"))))
            QDesktopServices::openUrl(QUrl(QStringLiteral("https://apps.microsoft.com/detail/9nblggh4nns1")));
    });
    connect(ui->botonRevisar, &QPushButton::clicked, this, [this] {
        if (revisarWinget())
            tema::mostrarEstado(ui->etiquetaEstado, tr("¡Listo! winget ya está."), tema::Estado::Ok);
        else
            tema::mostrarEstado(ui->etiquetaEstado, tr("Todavía no lo encuentro. Si recién lo instalaste, cerrá y volvé a abrir la app."),
                                tema::Estado::Aviso);
    });

    proceso.setProcessChannelMode(QProcess::MergedChannels);
    connect(&proceso, &QProcess::readyReadStandardOutput, this, &PaginaProgramas::leerSalida);
    connect(&proceso, &QProcess::finished, this, &PaginaProgramas::termino);
    connect(&proceso, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        tarea = Tarea::Ninguna;
        habilitar(true);
        rutaWinget.clear();
        ui->grupoFalta->show();
        tema::mostrarEstado(ui->etiquetaEstado, tr("No se pudo abrir winget: %1").arg(proceso.errorString()), tema::Estado::Error);
    });
}

PaginaProgramas::~PaginaProgramas()
{
    delete ui;
}

// La primera vez que se muestra la página se fija si está winget (no antes: no hace falta al abrir la app)
void PaginaProgramas::showEvent(QShowEvent *evento)
{
    QWidget::showEvent(evento);
    if (!revisado) {
        revisado = true;
        revisarWinget();
    }
}

bool PaginaProgramas::revisarWinget()
{
    rutaWinget = buscarWinget();
    ui->grupoFalta->setVisible(rutaWinget.isEmpty());
    ui->grupoWinget->setEnabled(!rutaWinget.isEmpty());
    return !rutaWinget.isEmpty();
}

bool PaginaProgramas::ocupada() const
{
    return proceso.state() != QProcess::NotRunning;
}

void PaginaProgramas::cancelarYEsperar()
{
    if (!ocupada())
        return;
    cancelado = true;
    Herramientas::cortar(proceso);
    proceso.waitForFinished(5000);
}

QString PaginaProgramas::idElegido() const
{
    const QTreeWidgetItem *item = ui->arbolPaquetes->currentItem();
    return item ? item->data(0, RolId).toString() : QString();
}

void PaginaProgramas::actualizarBotones()
{
    const bool libre = !ocupada();
    const bool elegido = ui->arbolPaquetes->currentItem() && !ui->arbolPaquetes->selectedItems().isEmpty();
    ui->botonInstalar->setEnabled(libre && elegido && mostrando == Tarea::Buscar);
    ui->botonActualizar->setEnabled(libre && elegido && mostrando == Tarea::Actualizables);
    ui->botonActualizarTodo->setEnabled(libre && mostrando == Tarea::Actualizables && ui->arbolPaquetes->topLevelItemCount() > 0);
}

void PaginaProgramas::habilitar(bool habilitado)
{
    ui->editBuscar->setEnabled(habilitado);
    ui->botonBuscar->setEnabled(habilitado);
    ui->botonActualizables->setEnabled(habilitado);
    ui->arbolPaquetes->setEnabled(habilitado);
    ui->botonCancelar->setEnabled(!habilitado);
    actualizarBotones();
    emit ocupacionCambiada();
}

void PaginaProgramas::correr(Tarea nueva, const QStringList &argumentos, const QString &estado)
{
    if (ocupada())
        return;
    if (rutaWinget.isEmpty() && !revisarWinget()) {
        tema::mostrarEstado(ui->etiquetaEstado, tr("Falta winget (ver arriba)."), tema::Estado::Error);
        return;
    }
    tarea = nueva;
    cancelado = false;
    salida.clear();
    pendiente.clear();
    if (tarea == Tarea::Instalar || tarea == Tarea::Actualizar)
        ui->registro->clear();
    tema::mostrarEstado(ui->etiquetaEstado, estado);
    habilitar(false);
    ui->registro->appendPlainText(QStringLiteral("> winget ") + argumentos.join(u' '));
    proceso.start(rutaWinget, argumentos);
    proceso.closeWriteChannel();    // si winget preguntara algo, recibe «fin» en vez de quedarse esperando
}

void PaginaProgramas::leerSalida()
{
    const QByteArray llegado = proceso.readAll();
    salida += llegado;
    pendiente += llegado;
    qsizetype inicio = 0;
    for (qsizetype i = 0; i < pendiente.size(); ++i) {
        if (pendiente.at(i) == '\n') {
            procesarLinea(winget::limpiar(pendiente.mid(inicio, i - inicio)));
            inicio = i + 1;
        }
    }
    pendiente.remove(0, inicio);
    // La barra de progreso se reescribe con \r sin terminar la línea: se muestra lo último
    if (!pendiente.isEmpty()) {
        const QString ultimo = winget::limpiar(pendiente).trimmed();
        if (winget::esProgreso(ultimo))
            tema::mostrarEstado(ui->etiquetaEstado, ultimo);
    }
}

void PaginaProgramas::procesarLinea(const QString &linea)
{
    if (linea.trimmed().isEmpty())
        return;
    if (winget::esProgreso(linea)) {
        tema::mostrarEstado(ui->etiquetaEstado, linea.trimmed());
        return;
    }
    // Las tablas de búsqueda y actualizables se leen enteras al terminar
    if (tarea == Tarea::Instalar || tarea == Tarea::Actualizar)
        ui->registro->appendPlainText(linea);
}

void PaginaProgramas::termino(int codigo, QProcess::ExitStatus estado)
{
    leerSalida();
    if (!pendiente.isEmpty()) {
        procesarLinea(winget::limpiar(pendiente));
        pendiente.clear();
    }
    const Tarea hecha = tarea;
    tarea = Tarea::Ninguna;
    const QString mensaje = winget::mensajeCodigo(codigo);

    if (cancelado) {
        tema::mostrarEstado(ui->etiquetaEstado, tr("Cancelado."));
    } else if (estado == QProcess::CrashExit) {
        tema::mostrarEstado(ui->etiquetaEstado, tr("winget se cerró de golpe. Volvé a intentarlo."), tema::Estado::Error);
    } else if (hecha == Tarea::Buscar || hecha == Tarea::Actualizables) {
        const QString texto = winget::limpiar(salida);
        const QList<winget::Paquete> paquetes = winget::leerTabla(texto);
        ui->arbolPaquetes->clear();
        mostrando = hecha;
        ui->arbolPaquetes->setColumnHidden(3, hecha != Tarea::Actualizables);
        for (const winget::Paquete &p : paquetes) {
            auto *item = new QTreeWidgetItem(ui->arbolPaquetes, {p.nombre, p.id, p.version, p.disponible});
            item->setData(0, RolId, p.id);
        }
        for (int c = 1; c < 4; ++c)
            ui->arbolPaquetes->resizeColumnToContents(c);
        if (!paquetes.isEmpty()) {
            ui->arbolPaquetes->setCurrentItem(ui->arbolPaquetes->topLevelItem(0));
            tema::mostrarEstado(ui->etiquetaEstado, hecha == Tarea::Buscar
                ? tr("Encontré %n programa(s). Elegí uno y tocá «Instalar».", nullptr, int(paquetes.size()))
                : tr("Hay %n programa(s) con versión nueva.", nullptr, int(paquetes.size())), tema::Estado::Ok);
        } else if (hecha == Tarea::Actualizables && (codigo == 0 || quint32(codigo) == 0x8A15002Bu)) {
            tema::mostrarEstado(ui->etiquetaEstado, tr("Todos los programas están al día."), tema::Estado::Ok);
        } else {
            // Sin tabla: se muestra lo que dijo winget
            ui->registro->appendPlainText(texto.trimmed());
            tema::mostrarEstado(ui->etiquetaEstado, mensaje.isEmpty() ? tr("No encontré nada.") : mensaje,
                                codigo == 0 || quint32(codigo) == 0x8A150014u ? tema::Estado::Aviso : tema::Estado::Error);
        }
    } else if (codigo == 0) {
        tema::mostrarEstado(ui->etiquetaEstado, hecha == Tarea::Instalar ? tr("Instalado.") : tr("Actualizado."), tema::Estado::Ok);
    } else {
        tema::mostrarEstado(ui->etiquetaEstado, mensaje, quint32(codigo) == 0x8A15002Bu ? tema::Estado::Aviso : tema::Estado::Error);
    }
    habilitar(true);

    // Después de actualizar, la lista de actualizables cambió
    if (hecha == Tarea::Actualizar && !cancelado && mostrando == Tarea::Actualizables) {
        const QString aviso = ui->etiquetaEstado->text();
        correr(Tarea::Actualizables, winget::argumentosActualizables(), aviso + tr(" Revisando qué falta actualizar…"));
    }
}
