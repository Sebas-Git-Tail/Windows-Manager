#include "VentanaPrincipal.h"
#include <QHBoxLayout>
#include "ui_VentanaPrincipal.h"

#include "PanelApps.h"
#include "VentanaNovedades.h"
#include "Piezas.h"
#include "Tema.h"
#include "ConfigServidor.h"
#include "Herramientas.h"
#include "PaginaDescargas.h"
#include "PaginaRespaldo.h"

#include <QApplication>
#include <QCloseEvent>
#include <QUrl>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>

#include <windows.h>

VentanaPrincipal::VentanaPrincipal(QWidget *parent)
    : QWidget(parent), ui(new Ui::VentanaPrincipal),
      videos(new PaginaDescargas(PaginaDescargas::Tipo::Videos, this)),
      musica(new PaginaDescargas(PaginaDescargas::Tipo::Musica, this)),
      respaldo(new PaginaRespaldo(this))
{
    ui->setupUi(this);

    // Estilo común (comun/ui): título grande, menú lateral con íconos y avisos de color
    ui->etiquetaTitulo->setProperty("titulo", true);
    ui->listaSecciones->setProperty("lateral", true);
    ui->listaSecciones->setIconSize(QSize(18, 18));
    ui->etiquetaVersion->setProperty("secundario", true);
    ui->botonActualizarYtdlp->setIcon(tema::icono(QStringLiteral("actualizar")));
    const auto estiloAviso = [this] {
        const tema::Colores &c = tema::colores();
        ui->etiquetaAviso->setStyleSheet(QStringLiteral("QLabel { background: rgba(%1, %2, %3, 0.14); color: %4; border: 1px solid rgba(%1, %2, %3, 0.4);"
                                                        " border-radius: 8px; padding: 8px 12px; }")
                                             .arg(c.aviso.red()).arg(c.aviso.green()).arg(c.aviso.blue()).arg(c.texto.name()));
    };
    estiloAviso();
    tema::alCambiarModo(this, estiloAviso);
    // Arriba a la derecha, al lado del título: modo claro / oscuro (se recuerda)
    auto *encabezado = new QHBoxLayout;
    ui->layoutContenido->removeWidget(ui->etiquetaTitulo);
    encabezado->addWidget(ui->etiquetaTitulo, 1);
    encabezado->addWidget(soporte::botonNovedades(this));   // novedades e informe de fallas
    encabezado->addWidget(tema::botonModo(this));
    ui->layoutContenido->insertLayout(0, encabezado);
    ui->etiquetaLogo->setPixmap(QPixmap(QStringLiteral(":/Logo.png")));

    // «Apps»: actualizar Windows-Manager e instalar las otras apps (comun/apps)
    auto *paginaApps = new apps::PanelApps(QStringLiteral("Windows-Manager"), this);
    const QList<QPair<QWidget *, QString>> paginas{{videos, QStringLiteral("video")}, {musica, QStringLiteral("musica")},
                                                   {respaldo, QStringLiteral("respaldo")}, {paginaApps, QStringLiteral("paquete")}};
    for (const auto &[pagina, icono] : paginas) {
        ui->paginas->addWidget(pagina);
        auto *item = new QListWidgetItem(tema::icono(icono, tema::colores().texto2), pagina->windowTitle(), ui->listaSecciones);
        item->setSizeHint(QSize(0, 38));
    }
    connect(ui->listaSecciones, &QListWidget::currentRowChanged, this, [this](int fila) {
        ui->paginas->setCurrentIndex(fila);
        ui->etiquetaTitulo->setText(ui->paginas->currentWidget()->windowTitle());
    });
    ui->listaSecciones->setCurrentRow(0);

    connect(videos, &PaginaDescargas::ocupacionCambiada, this, &VentanaPrincipal::actualizarEnergia);
    connect(musica, &PaginaDescargas::ocupacionCambiada, this, &VentanaPrincipal::actualizarEnergia);
    connect(respaldo, &PaginaRespaldo::ocupacionCambiada, this, &VentanaPrincipal::actualizarEnergia);

    ui->etiquetaAviso->hide();
    const QStringList faltan = Herramientas::faltantes();
    if (!faltan.isEmpty())
        avisar(tr("Faltan programas en la carpeta «herramientas»: %1").arg(faltan.join(QStringLiteral(", ")).toHtmlEscaped()));

    ui->etiquetaVersion->setText(tr("Versión %1").arg(QApplication::applicationVersion()));
    connect(ui->botonActualizarYtdlp, &QPushButton::clicked, this, &VentanaPrincipal::actualizarYtDlp);

    // Datos del servidor (IPs, carpeta de respaldos) desde servidor.json, una vez por día
    ConfigServidor::instancia().actualizar();

    // ¿Hay una versión nueva? (una vez por día): pregunta y la instala sola
    apps::avisarActualizaciones(this, QStringLiteral("Windows-Manager"));
    soporte::novedadesAlAbrir(this);    // la primera vez que se abre una versión nueva: qué cambió
}

// Aviso arriba de la página (se suman si hay más de uno)
void VentanaPrincipal::avisar(const QString &html)
{
    const QString actual = ui->etiquetaAviso->isVisible() ? ui->etiquetaAviso->text() : QString();
    ui->etiquetaAviso->setText(actual.isEmpty() ? html : actual + QStringLiteral("<br>") + html);
    ui->etiquetaAviso->show();
}

// yt-dlp se actualiza solo una vez por día antes de descargar; esto lo fuerza ahora
void VentanaPrincipal::actualizarYtDlp()
{
    if (procesoYtDlp.state() != QProcess::NotRunning || videos->ocupada() || musica->ocupada()) {
        tema::avisar(this, tr("Esperá a que terminen las descargas para actualizar yt-dlp."), tema::Estado::Aviso);
        return;
    }
    ui->botonActualizarYtdlp->setEnabled(false);
    ui->botonActualizarYtdlp->setText(tr("Actualizando…"));
    procesoYtDlp.setProcessChannelMode(QProcess::MergedChannels);
    procesoYtDlp.setProcessEnvironment(Herramientas::ambiente());
    connect(&procesoYtDlp, &QProcess::finished, this, [this](int codigo, QProcess::ExitStatus estado) {
        procesoYtDlp.disconnect(this);
        ui->botonActualizarYtdlp->setEnabled(true);
        ui->botonActualizarYtdlp->setText(tr("Actualizar yt-dlp"));
        const QString salida = QString::fromLocal8Bit(procesoYtDlp.readAll()).trimmed();
        if (estado == QProcess::NormalExit && codigo == 0) {
            Herramientas::marcarYtDlpActualizado();
            const QString ultima = salida.section(u'\n', -1).trimmed();
            tema::avisar(this, ultima.isEmpty() ? tr("yt-dlp está actualizado.") : ultima, tema::Estado::Ok);
        } else {
            QMessageBox::warning(this, windowTitle(), tr("No se pudo actualizar yt-dlp:\n%1").arg(salida.right(600)));
        }
    });
    procesoYtDlp.start(Herramientas::programa(QStringLiteral("yt-dlp.exe")), {QStringLiteral("--update")});
}

VentanaPrincipal::~VentanaPrincipal()
{
    delete ui;
}

bool VentanaPrincipal::hayTarea() const
{
    return videos->ocupada() || musica->ocupada() || respaldo->ocupada();
}

// Que la computadora no se suspenda en medio de una descarga o un respaldo
void VentanaPrincipal::actualizarEnergia()
{
    SetThreadExecutionState(hayTarea() ? (ES_CONTINUOUS | ES_SYSTEM_REQUIRED) : ES_CONTINUOUS);
}

void VentanaPrincipal::closeEvent(QCloseEvent *evento)
{
    if (hayTarea()) {
        QMessageBox caja(QMessageBox::Question, windowTitle(), tr("Hay una descarga o un respaldo en curso. ¿Cancelarlo y salir?"), QMessageBox::NoButton, this);
        const QPushButton *salir = caja.addButton(tr("Cancelar y salir"), QMessageBox::AcceptRole);
        caja.setDefaultButton(caja.addButton(tr("Seguir"), QMessageBox::RejectRole));
        caja.exec();
        if (caja.clickedButton() != salir) {
            evento->ignore();
            return;
        }
        videos->cancelarYEsperar();
        musica->cancelarYEsperar();
        respaldo->cancelarYEsperar();
    }
    evento->accept();
}
