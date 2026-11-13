#include "VentanaPrincipal.h"
#include "ui_VentanaPrincipal.h"

#include "Herramientas.h"
#include "PaginaDescargas.h"
#include "PaginaRespaldo.h"

#include <QCloseEvent>
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

    const QStringList faltan = Herramientas::faltantes();
    if (!faltan.isEmpty()) {
        ui->etiquetaAviso->setText(tr("× Faltan programas en la carpeta «herramientas»: %1").arg(faltan.join(QStringLiteral(", "))));
        ui->etiquetaAviso->show();
    } else {
        ui->etiquetaAviso->hide();
    }
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
