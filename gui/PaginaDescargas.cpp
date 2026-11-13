#include "PaginaDescargas.h"
#include "ui_PaginaDescargas.h"

#include "Herramientas.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include <string>

#include <windows.h>
#include <shellapi.h>

constexpr int RolRuta = Qt::UserRole;

PaginaDescargas::PaginaDescargas(Tipo tipo, QWidget *parent)
    : QWidget(parent), ui(new Ui::PaginaDescargas), tipo(tipo)
{
    ui->setupUi(this);
    const bool musica = tipo == Tipo::Musica;

    if (musica) {
        setWindowTitle(tr("Descargar Música"));
        ui->grupoUrls->setTitle(tr("URLs de YouTube Music (una por línea)"));
        ui->editUrls->setPlaceholderText(QStringLiteral("https://music.youtube.com/watch?v=..."));
        ui->grupoDescargados->setTitle(tr("Canciones (en Música, una carpeta por artista)"));
        ui->botonVer->setText(tr("Reproducir"));
    } else {
        setWindowTitle(tr("Descargar Vídeos"));
    }
    ui->filaPlataforma->setVisible(!musica);
    ui->checkPlaylist->setVisible(musica);
    ui->botonYoutubeMusic->setVisible(musica);
    ui->editCarpeta->setText(QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)));
    ui->filaCarpeta->setVisible(false);
    ui->barraArchivo->hide();
    ui->barraTotal->hide();

    connect(ui->comboPlataforma, &QComboBox::currentIndexChanged, this,
            [this](int plataforma) { ui->filaCarpeta->setVisible(plataforma == 1); });    // 1 = Redes sociales
    connect(ui->botonElegirCarpeta, &QPushButton::clicked, this, &PaginaDescargas::elegirCarpeta);
    connect(ui->botonPegar, &QPushButton::clicked, this, [this] {
        const QString texto = QGuiApplication::clipboard()->text().trimmed();
        if (texto.isEmpty())
            return;
        if (!ui->editUrls->toPlainText().trimmed().isEmpty())
            ui->editUrls->appendPlainText({});
        ui->editUrls->insertPlainText(texto);
    });
    connect(ui->botonYoutubeMusic, &QPushButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://music.youtube.com"))); });
    connect(ui->botonDescargar, &QPushButton::clicked, this, &PaginaDescargas::descargar);
    connect(ui->botonCancelar, &QPushButton::clicked, this, &PaginaDescargas::cancelar);
    connect(ui->botonVer, &QPushButton::clicked, this, &PaginaDescargas::ver);
    connect(ui->botonAbrirCarpeta, &QPushButton::clicked, this, &PaginaDescargas::abrirCarpeta);
    connect(ui->botonPapelera, &QPushButton::clicked, this, &PaginaDescargas::moverAPapelera);
    connect(ui->listaVideos, &QListWidget::itemDoubleClicked, this, &PaginaDescargas::ver);
    connect(ui->listaVideos, &QListWidget::itemSelectionChanged, this, [this] {
        const bool hay = !ui->listaVideos->selectedItems().isEmpty();
        ui->botonVer->setEnabled(hay);
        ui->botonAbrirCarpeta->setEnabled(hay);
        ui->botonPapelera->setEnabled(hay);
    });

    proceso.setProcessChannelMode(QProcess::MergedChannels);
    connect(&proceso, &QProcess::readyReadStandardOutput, this, &PaginaDescargas::leerSalida);
    connect(&proceso, &QProcess::finished, this, &PaginaDescargas::terminoProceso);
    connect(&proceso, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        anotar(tr("× No se pudo ejecutar yt-dlp: %1").arg(proceso.errorString()));
        cola.clear();
        fallidas += qMax(1, total - indice);
        finalizar();
    });
}

PaginaDescargas::~PaginaDescargas()
{
    delete ui;
}

bool PaginaDescargas::ocupada() const
{
    return !ui->botonDescargar->isEnabled();
}

void PaginaDescargas::cancelarYEsperar()
{
    cancelar();
    proceso.waitForFinished(5000);
}

// Música va a Música; YouTube a Vídeos/Yt-dlp; redes sociales a la carpeta elegida
QString PaginaDescargas::destino() const
{
    if (tipo == Tipo::Musica)
        return QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    if (ui->comboPlataforma->currentIndex() == 0)
        return QDir::toNativeSeparators(QDir(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)).filePath(QStringLiteral("Yt-dlp")));
    return ui->editCarpeta->text().trimmed();
}

void PaginaDescargas::elegirCarpeta()
{
    const QString carpeta = QFileDialog::getExistingDirectory(this, tr("Guardar en"), ui->editCarpeta->text());
    if (!carpeta.isEmpty())
        ui->editCarpeta->setText(QDir::toNativeSeparators(carpeta));
}

// Para yt-dlp: --cookies-from-browser. Prioridad: LibreWolf y Firefox. Los navegadores tipo Chrome
// y Edge cifran sus cookies en Windows de una forma que yt-dlp no puede leer.
QString PaginaDescargas::navegadorCookies() const
{
    const QDir perfiles(qEnvironmentVariable("APPDATA"));
    const QString libreWolf = perfiles.filePath(QStringLiteral("librewolf/Profiles"));
    if (QFileInfo::exists(libreWolf))
        return QStringLiteral("firefox:") + QDir::toNativeSeparators(libreWolf);
    if (QFileInfo::exists(perfiles.filePath(QStringLiteral("Mozilla/Firefox/Profiles"))))
        return QStringLiteral("firefox");
    return {};
}

QString PaginaDescargas::carpetaMiniaturas() const
{
    return QDir(Herramientas::temporal()).filePath(QStringLiteral("miniaturas"));
}

QString PaginaDescargas::archivoRutas() const
{
    return QDir(Herramientas::temporal()).filePath(tipo == Tipo::Musica ? QStringLiteral("rutas-musica.txt")
                                                                        : QStringLiteral("rutas-videos.txt"));
}

void PaginaDescargas::habilitarFormulario(bool habilitado)
{
    ui->grupoUrls->setEnabled(habilitado);
    ui->grupoOpciones->setEnabled(habilitado);
    ui->botonDescargar->setEnabled(habilitado);
    ui->botonCancelar->setEnabled(!habilitado);
    // El botón deshabilitado pierde el foco: así Cancelar sigue a mano con el teclado
    (habilitado ? ui->botonDescargar : ui->botonCancelar)->setFocus();
    emit ocupacionCambiada();
}

void PaginaDescargas::anotar(const QString &texto)
{
    ui->registro->appendPlainText(texto);
}

void PaginaDescargas::descargar()
{
    QStringList urls;
    const QStringList lineas = ui->editUrls->toPlainText().split(u'\n');
    for (const QString &linea : lineas) {
        if (!linea.trimmed().isEmpty())
            urls << linea.trimmed();
    }
    if (urls.isEmpty()) {
        ui->etiquetaEstado->setText(tr("No se ingresó ninguna URL."));
        return;
    }
    if (destino().isEmpty()) {
        ui->etiquetaEstado->setText(tr("Elegí una carpeta de destino."));
        return;
    }
    if (ui->checkCookies->isChecked() && navegadorCookies().isEmpty()) {
        ui->etiquetaEstado->setText(tr("× Para usar cookies hace falta LibreWolf o Firefox con la sesión iniciada; las de Chrome y Edge no se pueden leer."));
        return;
    }
    if (!QDir().mkpath(destino())) {
        ui->etiquetaEstado->setText(tr("× No se pudo crear la carpeta %1.").arg(destino()));
        return;
    }
    QDir(carpetaMiniaturas()).removeRecursively();
    QDir().mkpath(carpetaMiniaturas());

    cola = urls;
    total = urls.size();
    indice = listas = fallidas = 0;
    cancelado = false;
    ui->barraArchivo->setValue(0);
    ui->barraTotal->setRange(0, total);
    ui->barraTotal->setValue(0);
    ui->barraArchivo->show();
    ui->barraTotal->show();
    habilitarFormulario(false);

    proceso.setProcessEnvironment(Herramientas::ambiente());

    // YouTube cambia seguido y un yt-dlp viejo deja de funcionar
    if (Herramientas::ytDlpDesactualizado()) {
        actualizando = true;
        ui->etiquetaEstado->setText(tr("Buscando actualizaciones de yt-dlp…"));
        proceso.start(Herramientas::programa(QStringLiteral("yt-dlp.exe")), {QStringLiteral("--update")});
        return;
    }
    siguienteUrl();
}

void PaginaDescargas::siguienteUrl()
{
    actualizando = false;
    if (cola.isEmpty() || cancelado) {
        finalizar();
        return;
    }
    const QString url = cola.takeFirst();
    ++indice;
    mensajeError.clear();
    ultimoPorcentaje.clear();
    QFile::remove(archivoRutas());
    ui->barraTotal->setValue(indice - 1);
    ui->barraArchivo->setValue(0);
    ui->etiquetaEstado->setText(tr("URL %1 de %2: %3").arg(indice).arg(total).arg(url));

    // Las opciones de Logica/Descargar.sh
    QStringList argumentos = Herramientas::argumentosYtDlp();
    argumentos << QStringLiteral("--write-thumbnail")
               << QStringLiteral("--convert-thumbnails") << QStringLiteral("jpg")    // la lista solo sabe mostrar jpg
               << QStringLiteral("--audio-quality") << QStringLiteral("0")
               << QStringLiteral("--embed-metadata")
               << QStringLiteral("--embed-thumbnail")
               << QStringLiteral("--prefer-free-formats")
               << QStringLiteral("--no-mtime")    // la fecha del archivo es la de la descarga
               << QStringLiteral("-o") << QStringLiteral("%(artist,uploader)s/%(title)s.%(ext)s")
               << QStringLiteral("-o") << QStringLiteral("thumbnail:%1/%(id)s.%(ext)s").arg(carpetaMiniaturas())
               << QStringLiteral("--print-to-file") << QStringLiteral("after_move:filepath") << archivoRutas()
               << QStringLiteral("--newline")
               << QStringLiteral("--progress")
               << QStringLiteral("-P") << destino();
    if (tipo == Tipo::Musica) {
        argumentos << QStringLiteral("-x");
        if (!ui->checkPlaylist->isChecked())
            argumentos << QStringLiteral("--no-playlist");
    }
    if (ui->checkCookies->isChecked())
        argumentos << QStringLiteral("--cookies-from-browser") << navegadorCookies();
    argumentos << QStringLiteral("--") << url;

    proceso.start(Herramientas::programa(QStringLiteral("yt-dlp.exe")), argumentos);
}

void PaginaDescargas::cancelar()
{
    if (!ocupada())
        return;
    cancelado = true;
    cola.clear();
    ui->etiquetaEstado->setText(tr("Cancelando…"));
    Herramientas::cortar(proceso);
}

void PaginaDescargas::leerSalida()
{
    while (proceso.canReadLine())
        procesarLinea(QString::fromUtf8(proceso.readLine()).trimmed());
}

void PaginaDescargas::procesarLinea(const QString &linea)
{
    static const QRegularExpression porcentaje(QStringLiteral("(\\d+)(\\.\\d+)?%"));
    static const QString prefijoDestino = QStringLiteral("[download] Destination: ");

    if (actualizando) {
        if (!linea.isEmpty())
            anotar(linea);
    } else if (linea.startsWith(prefijoDestino)) {
        archivoActual = QFileInfo(linea.mid(prefijoDestino.size())).fileName();
        ultimoPorcentaje.clear();
        ui->barraArchivo->setValue(0);
        ui->etiquetaEstado->setText(tr("Descargando %1").arg(archivoActual));
    } else if (linea.startsWith(QStringLiteral("[download]"))) {
        const QRegularExpressionMatch coincidencia = porcentaje.match(linea);
        if (coincidencia.hasMatch() && coincidencia.captured(1) != ultimoPorcentaje) {
            ultimoPorcentaje = coincidencia.captured(1);
            ui->barraArchivo->setValue(ultimoPorcentaje.toInt());
            if (ultimoPorcentaje.toInt() >= 100)
                ui->etiquetaEstado->setText(tr("Procesando %1 (metadatos y carátula)…").arg(archivoActual));
        }
    } else if (linea.startsWith(QStringLiteral("ERROR: "))) {
        mensajeError = linea.mid(7);
    } else if (linea.startsWith(QStringLiteral("WARNING: "))) {
        anotar(linea);
    }
}

QString PaginaDescargas::ultimaMiniatura() const
{
    const QFileInfoList miniaturas = QDir(carpetaMiniaturas()).entryInfoList({QStringLiteral("*.jpg"), QStringLiteral("*.png")},
                                                                            QDir::Files, QDir::Time);
    return miniaturas.isEmpty() ? QString() : miniaturas.constFirst().absoluteFilePath();
}

// Lo que se llegó a bajar se muestra aunque después algo haya fallado (playlists)
void PaginaDescargas::registrarTerminados(const QString &miniatura)
{
    QFile archivo(archivoRutas());
    if (!archivo.open(QIODevice::ReadOnly))
        return;
    // En Windows, yt-dlp termina cada línea con \r\n: el \r haría que el archivo no exista
    const QStringList lineas = QString::fromUtf8(archivo.readAll()).split(u'\n', Qt::SkipEmptyParts);
    for (const QString &linea : lineas) {
        const QString ruta = linea.trimmed();
        if (ruta.isEmpty())
            continue;
        const QFileInfo info(ruta);
        const QString nombre = tipo == Tipo::Musica ? info.dir().dirName() + QStringLiteral(" — ") + info.completeBaseName()
                                                    : info.completeBaseName();
        auto *item = new QListWidgetItem(QIcon(miniatura), nombre, ui->listaVideos);
        item->setData(RolRuta, ruta);
        ui->listaVideos->clearSelection();
        ui->listaVideos->setCurrentItem(item);
        item->setSelected(true);
        ++listas;
    }
}

void PaginaDescargas::terminoProceso(int codigo, QProcess::ExitStatus estado)
{
    leerSalida();
    if (actualizando) {
        // Si no se pudo actualizar se sigue con la versión que hay
        if (codigo == 0 && estado == QProcess::NormalExit)
            Herramientas::marcarYtDlpActualizado();
        else if (!cancelado)
            anotar(tr("No se pudo actualizar yt-dlp; se usa la versión instalada."));
        siguienteUrl();
        return;
    }

    const int antes = listas;
    registrarTerminados(ultimaMiniatura());
    if (!cancelado && (codigo != 0 || estado != QProcess::NormalExit || listas == antes)) {
        ++fallidas;
        anotar(tr("× Falló: %1").arg(mensajeError.isEmpty() ? tr("yt-dlp terminó con código %1").arg(codigo) : mensajeError));
        if (tipo == Tipo::Musica && !ui->checkCookies->isChecked())
            anotar(tr("   Puede deberse a la falta de cookies (probá con «Usar cookies del navegador»)."));
    }
    ui->barraTotal->setValue(indice);
    siguienteUrl();
}

void PaginaDescargas::finalizar()
{
    habilitarFormulario(true);
    ui->barraTotal->setValue(ui->barraTotal->maximum());

    const bool musica = tipo == Tipo::Musica;
    if (cancelado)
        ui->etiquetaEstado->setText(tr("Descarga cancelada."));
    else if (fallidas > 0)
        ui->etiquetaEstado->setText(tr("Terminado con errores: %1 descargado(s), %2 URL(s) fallaron.").arg(listas).arg(fallidas));
    else if (listas == 1)
        ui->etiquetaEstado->setText(musica ? tr("✓ Listo: 1 canción descargada.") : tr("✓ Listo: 1 video descargado."));
    else
        ui->etiquetaEstado->setText((musica ? tr("✓ Listo: %1 canciones descargadas.") : tr("✓ Listo: %1 videos descargados.")).arg(listas));
}

void PaginaDescargas::ver()
{
    const QListWidgetItem *item = ui->listaVideos->currentItem();
    if (!item)
        return;
    const QString ruta = item->data(RolRuta).toString();
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(ruta)))
        return;

    // Sin programa asociado (pasa seguido con .opus y .mkv): que elija con «Abrir con» de Windows
    const std::wstring nativa = QDir::toNativeSeparators(ruta).toStdWString();
    const auto resultado = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"openas", nativa.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (resultado <= 32)
        ui->etiquetaEstado->setText(tr("× No se pudo abrir el archivo (error %1).").arg(resultado));
}

void PaginaDescargas::abrirCarpeta()
{
    if (const QListWidgetItem *item = ui->listaVideos->currentItem())
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(item->data(RolRuta).toString()).absolutePath()));
}

// A la papelera y no borrado definitivo: si fue un error, se puede recuperar
void PaginaDescargas::moverAPapelera()
{
    QListWidgetItem *item = ui->listaVideos->currentItem();
    if (!item)
        return;
    QMessageBox caja(QMessageBox::Question, windowTitle(), tr("¿Mover «%1» a la papelera?").arg(item->text()),
                     QMessageBox::NoButton, this);
    const QPushButton *si = caja.addButton(tr("Mover"), QMessageBox::AcceptRole);
    caja.setDefaultButton(caja.addButton(tr("No"), QMessageBox::RejectRole));
    caja.exec();
    if (caja.clickedButton() != si)
        return;

    QFile archivo(item->data(RolRuta).toString());
    if (archivo.moveToTrash() || !archivo.exists())
        delete item;
    else
        anotar(tr("No se pudo mover a la papelera: %1").arg(archivo.fileName()));
}
