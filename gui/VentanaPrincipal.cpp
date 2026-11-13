#include "VentanaPrincipal.h"
#include "ui_VentanaPrincipal.h"

#include "Actualizaciones.h"
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

    QFont fuente = ui->etiquetaTitulo->font();
    fuente.setPointSizeF(fuente.pointSizeF() * 1.4);
    fuente.setBold(true);
    ui->etiquetaTitulo->setFont(fuente);
    ui->etiquetaLogo->setPixmap(QPixmap(QStringLiteral(":/Logo.png")));

    for (QWidget *pagina : {static_cast<QWidget *>(videos), static_cast<QWidget *>(musica), static_cast<QWidget *>(respaldo)}) {
        ui->paginas->addWidget(pagina);
        ui->listaSecciones->addItem(pagina->windowTitle());
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
        avisar(tr("× Faltan programas en la carpeta «herramientas»: %1").arg(faltan.join(QStringLiteral(", ")).toHtmlEscaped()));

    ui->etiquetaVersion->setText(tr("Versión %1").arg(QApplication::applicationVersion()));
    connect(ui->botonActualizarYtdlp, &QPushButton::clicked, this, &VentanaPrincipal::actualizarYtDlp);

    // Datos del servidor (IPs, carpeta de respaldos) desde servidor.json, una vez por día
    ConfigServidor::instancia().actualizar();

    // ¿Hay una versión nueva en GitHub? (una vez por día)
    auto *actualizaciones = new Actualizaciones(QStringLiteral("Sebas-Git-Tail/Windows-Manager"), this);
    connect(actualizaciones, &Actualizaciones::hayVersionNueva, this, [this](const QString &version, const QUrl &url) {
        avisar(tr("Hay una versión nueva de Windows-Manager (%1): <a href=\"%2\">descargarla</a>.")
                   .arg(version.toHtmlEscaped(), url.toString(QUrl::FullyEncoded).toHtmlEscaped()));
    });
    actualizaciones->comprobar();
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
        avisar(tr("Esperá a que terminen las descargas para actualizar yt-dlp."));
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
            QMessageBox::information(this, windowTitle(), ultima.isEmpty() ? tr("yt-dlp está actualizado.") : ultima);
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
