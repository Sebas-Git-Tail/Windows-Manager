#include "PaginaDescargas.h"
#include "Tema.h"
#include "ui_PaginaDescargas.h"

#include "DialogoPistas.h"
#include "Herramientas.h"
#include "LogicaDescargas.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUrl>
#include <QUuid>

#include <string>

#include <windows.h>
#include <shellapi.h>

constexpr int RolRuta = Qt::UserRole;
constexpr int RolMiniatura = Qt::UserRole + 1;
constexpr int MaximoEnLista = 300;    // lo más viejo de la lista se olvida (los archivos no se tocan)

namespace {

QString sufijo(PaginaDescargas::Tipo tipo)
{
    switch (tipo) {
    case PaginaDescargas::Tipo::Musica: return QStringLiteral("musica");
    case PaginaDescargas::Tipo::Archivos: return QStringLiteral("archivos");
    case PaginaDescargas::Tipo::Videos: break;
    }
    return QStringLiteral("videos");
}

QSettings ajustes()
{
    return QSettings(Herramientas::archivoAjustes(), QSettings::IniFormat);
}

} // namespace

PaginaDescargas::PaginaDescargas(Tipo tipo, QWidget *parent)
    : QWidget(parent), ui(new Ui::PaginaDescargas), tipo(tipo)
{
    ui->setupUi(this);
    const bool archivos = tipo == Tipo::Archivos;
    const bool videos = tipo == Tipo::Videos;

    if (musica()) {
        setWindowTitle(tr("Música"));
        ui->grupoUrls->setTitle(tr("URLs de YouTube Music (una por línea)"));
        ui->editUrls->setPlaceholderText(QStringLiteral("https://music.youtube.com/watch?v=..."));
        ui->grupoDescargados->setTitle(tr("Canciones (en Música, una carpeta por artista)"));
        ui->botonVer->setText(tr("Reproducir"));
    } else if (archivos) {
        setWindowTitle(tr("Descargas en masa"));
        ui->grupoUrls->setTitle(tr("URLs de los archivos (una por línea)"));
        ui->editUrls->setPlaceholderText(QStringLiteral("https://..."));
        ui->editUrls->setMaximumHeight(160);
        ui->grupoDescargados->setTitle(tr("Archivos descargados (si ya hay uno con el mismo nombre, se guarda como «nombre (2)»)"));
        ui->listaVideos->setIconSize(QSize(24, 24));
        ui->botonVer->setText(tr("Abrir"));
    } else {
        setWindowTitle(tr("Videos"));
    }
    ui->filaPlataforma->setVisible(videos);
    ui->checkPlaylist->setVisible(musica());
    ui->checkPistas->setVisible(videos);
    ui->botonYoutubeMusic->setVisible(musica());
    ui->checkCookies->setVisible(!archivos);
    ui->checkServidor->setVisible(!archivos);
    ui->editCarpeta->setText(QDir::toNativeSeparators(QStandardPaths::writableLocation(
        archivos ? QStandardPaths::DownloadLocation : QStandardPaths::MoviesLocation)));
    ui->filaCarpeta->setVisible(archivos);
    ui->barraArchivo->hide();
    ui->barraTotal->hide();

    // Lo que se recuerda: bajar con el servidor y el archivo de cookies
    const QSettings guardado = ajustes();
    ui->checkServidor->setChecked(guardado.value(QStringLiteral("descargas/servidor-") + sufijo(tipo), false).toBool());
    ui->editCookies->setText(guardado.value(QStringLiteral("descargas/cookies")).toString());
    connect(ui->checkServidor, &QCheckBox::toggled, this, [this](bool activo) {
        ajustes().setValue(QStringLiteral("descargas/servidor-") + sufijo(this->tipo), activo);
        actualizarOpciones();
    });
    connect(ui->checkCookies, &QCheckBox::toggled, this, &PaginaDescargas::actualizarOpciones);
    connect(ui->botonElegirCookies, &QPushButton::clicked, this, &PaginaDescargas::elegirCookies);
    connect(ui->botonQuitarCookies, &QPushButton::clicked, this, [this] {
        ui->editCookies->clear();
        ajustes().remove(QStringLiteral("descargas/cookies"));
        actualizarOpciones();
    });
    actualizarOpciones();

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

    // Con el servidor: se pregunta cómo va cada un rato, y mientras se trae un archivo se muestra cuánto falta
    temporizadorRemoto.setSingleShot(true);
    temporizadorRemoto.setInterval(1500);
    connect(&temporizadorRemoto, &QTimer::timeout, this, &PaginaDescargas::remotoPreguntar);
    temporizadorTransferencia.setInterval(500);
    connect(&temporizadorTransferencia, &QTimer::timeout, this, &PaginaDescargas::remotoMostrarTransferencia);

    cargarDescargados();
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
    // Con el servidor, el pedido de cancelar sale recién con el bucle de eventos: si la ventana se cierra
    // antes, el servidor seguiría bajando. Se espera la respuesta un rato.
    if (cancelacionesPendientes > 0) {
        QEventLoop espera;
        QTimer limite;
        limite.setSingleShot(true);
        connect(&limite, &QTimer::timeout, &espera, &QEventLoop::quit);
        QTimer mirar;
        connect(&mirar, &QTimer::timeout, &espera, [this, &espera] {
            if (cancelacionesPendientes <= 0)
                espera.quit();
        });
        limite.start(5000);
        mirar.start(50);
        espera.exec(QEventLoop::ExcludeUserInputEvents);
    }
}

// Las cookies no hacen falta (ni se pueden usar) si baja el servidor, que usa las suyas
void PaginaDescargas::actualizarOpciones()
{
    const bool servidor = ui->checkServidor->isVisible() || tipo != Tipo::Archivos ? ui->checkServidor->isChecked() : false;
    ui->checkCookies->setEnabled(!servidor);
    ui->filaCookies->setVisible(tipo != Tipo::Archivos && ui->checkCookies->isChecked() && !servidor);
    ui->botonQuitarCookies->setEnabled(!ui->editCookies->text().isEmpty());
}

// Música va a Música; YouTube a Vídeos/Yt-dlp; redes sociales y archivos a la carpeta elegida
QString PaginaDescargas::destino() const
{
    if (musica())
        return QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    if (tipo == Tipo::Videos && ui->comboPlataforma->currentIndex() == 0)
        return QDir::toNativeSeparators(QDir(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)).filePath(QStringLiteral("Yt-dlp")));
    return ui->editCarpeta->text().trimmed();
}

void PaginaDescargas::elegirCarpeta()
{
    const QString carpeta = QFileDialog::getExistingDirectory(this, tr("Guardar en"), ui->editCarpeta->text());
    if (!carpeta.isEmpty())
        ui->editCarpeta->setText(QDir::toNativeSeparators(carpeta));
}

// Un cookies.txt exportado del navegador (por ejemplo con la extensión «Get cookies.txt»): sirve con
// cualquier navegador, también Chrome y Edge
void PaginaDescargas::elegirCookies()
{
    const QString archivo = QFileDialog::getOpenFileName(this, tr("Archivo de cookies"),
                                                         QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                                                         tr("Cookies (*.txt);;Todos los archivos (*)"));
    if (archivo.isEmpty())
        return;
    ui->editCookies->setText(QDir::toNativeSeparators(archivo));
    ajustes().setValue(QStringLiteral("descargas/cookies"), ui->editCookies->text());
    actualizarOpciones();
}

QString PaginaDescargas::archivoCookies() const
{
    return ui->editCookies->text().trimmed();
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

// yt-dlp vuelve a escribir el cookies.txt que recibe: se le pasa una copia, para no tocar el del usuario
QStringList PaginaDescargas::argumentosCookies() const
{
    if (!ui->checkCookies->isChecked())
        return {};
    if (archivoCookies().isEmpty())
        return {QStringLiteral("--cookies-from-browser"), navegadorCookies()};
    const QString copia = QDir(Herramientas::temporal()).filePath(QStringLiteral("cookies-%1.txt").arg(sufijo(tipo)));
    QFile::remove(copia);
    QFile::copy(archivoCookies(), copia);
    return {QStringLiteral("--cookies"), QDir::toNativeSeparators(copia)};
}

QString PaginaDescargas::carpetaSubtitulos() const
{
    return QDir(Herramientas::temporal()).filePath(QStringLiteral("subtitulos"));
}

QString PaginaDescargas::carpetaMiniaturas() const
{
    return QDir(Herramientas::temporal()).filePath(QStringLiteral("miniaturas"));
}

QString PaginaDescargas::archivoRutas() const
{
    return QDir(Herramientas::temporal()).filePath(QStringLiteral("rutas-%1.txt").arg(sufijo(tipo)));
}

// ── Lo descargado: queda en la lista aunque se cierre la app ──

QString PaginaDescargas::archivoLista() const
{
    return QDir(Herramientas::datos()).filePath(QStringLiteral("descargados-%1.json").arg(sufijo(tipo)));
}

QString PaginaDescargas::carpetaMiniaturasGuardadas() const
{
    const QString carpeta = QDir(Herramientas::datos()).filePath(QStringLiteral("miniaturas"));
    QDir().mkpath(carpeta);
    return carpeta;
}

QString PaginaDescargas::nuevaMiniatura() const
{
    return QDir(carpetaMiniaturasGuardadas()).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".jpg"));
}

void PaginaDescargas::cargarDescargados()
{
    QFile archivo(archivoLista());
    if (!archivo.open(QIODevice::ReadOnly))
        return;
    const QJsonArray lista = QJsonDocument::fromJson(archivo.readAll()).array();
    for (const QJsonValue &valor : lista) {
        const QJsonObject o = valor.toObject();
        const QString ruta = o.value(QStringLiteral("ruta")).toString();
        if (ruta.isEmpty() || !QFileInfo::exists(ruta))    // se borró o se movió desde afuera
            continue;
        const QString miniatura = o.value(QStringLiteral("miniatura")).toString();
        const QIcon icono = !miniatura.isEmpty() && QFileInfo::exists(miniatura) ? QIcon(miniatura)
                                                                                 : QFileIconProvider().icon(QFileInfo(ruta));
        auto *item = new QListWidgetItem(icono, o.value(QStringLiteral("nombre")).toString(), ui->listaVideos);
        item->setData(RolRuta, ruta);
        item->setData(RolMiniatura, miniatura);
    }
}

void PaginaDescargas::guardarDescargados()
{
    // Lo más viejo (arriba) se olvida, con su miniatura (es una copia propia de la app)
    while (ui->listaVideos->count() > MaximoEnLista) {
        QListWidgetItem *viejo = ui->listaVideos->takeItem(0);
        const QString miniatura = viejo->data(RolMiniatura).toString();
        delete viejo;
        borrarMiniaturaSiSobra(miniatura);
    }
    QJsonArray lista;
    for (int i = 0; i < ui->listaVideos->count(); ++i) {
        const QListWidgetItem *item = ui->listaVideos->item(i);
        lista.append(QJsonObject{{QStringLiteral("nombre"), item->text()},
                                 {QStringLiteral("ruta"), item->data(RolRuta).toString()},
                                 {QStringLiteral("miniatura"), item->data(RolMiniatura).toString()}});
    }
    QSaveFile archivo(archivoLista());
    if (archivo.open(QIODevice::WriteOnly)) {
        archivo.write(QJsonDocument(lista).toJson(QJsonDocument::Compact));
        archivo.commit();
    }
}

// Solo la copia propia de la app (nunca algo del usuario), y si ningún otro de la lista la usa: los
// archivos de una misma playlist comparten la miniatura
void PaginaDescargas::borrarMiniaturaSiSobra(const QString &miniatura)
{
    if (miniatura.isEmpty() || !miniatura.startsWith(carpetaMiniaturasGuardadas()))
        return;
    for (int i = 0; i < ui->listaVideos->count(); ++i) {
        if (ui->listaVideos->item(i)->data(RolMiniatura).toString() == miniatura)
            return;
    }
    QFile::remove(miniatura);
}

void PaginaDescargas::agregarDescargado(const QString &nombre, const QString &ruta, const QString &miniatura)
{
    const QIcon icono = miniatura.isEmpty() ? QFileIconProvider().icon(QFileInfo(ruta)) : QIcon(miniatura);
    auto *item = new QListWidgetItem(icono, nombre, ui->listaVideos);
    item->setData(RolRuta, ruta);
    item->setData(RolMiniatura, miniatura);
    ui->listaVideos->clearSelection();
    ui->listaVideos->setCurrentItem(item);
    item->setSelected(true);
    ui->listaVideos->scrollToItem(item);
    guardarDescargados();
}

void PaginaDescargas::habilitarFormulario(bool habilitado)
{
    ui->grupoUrls->setEnabled(habilitado);
    ui->grupoOpciones->setEnabled(habilitado);
    ui->filaCarpeta->setEnabled(habilitado);
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

void PaginaDescargas::ponerEstado(const QString &texto)
{
    tema::mostrarEstado(ui->etiquetaEstado, texto);
}

void PaginaDescargas::ponerProgreso(int archivo, int hechas)
{
    if (archivo < 0) {
        ui->barraArchivo->setRange(0, 0);
    } else {
        ui->barraArchivo->setRange(0, 100);
        ui->barraArchivo->setValue(archivo);
    }
    ui->barraTotal->setValue(hechas);
}

void PaginaDescargas::descargar()
{
    const QStringList urls = descargas::urlsDeTexto(ui->editUrls->toPlainText());
    if (urls.isEmpty()) {
        tema::mostrarEstado(ui->etiquetaEstado, tr("No se ingresó ninguna URL."), tema::Estado::Aviso);
        return;
    }
    if (destino().isEmpty()) {
        tema::mostrarEstado(ui->etiquetaEstado, tr("Elegí una carpeta de destino."), tema::Estado::Aviso);
        return;
    }
    remoto = tipo != Tipo::Archivos && ui->checkServidor->isChecked();
    if (!remoto && tipo != Tipo::Archivos && ui->checkCookies->isChecked()) {
        if (!archivoCookies().isEmpty() && !QFileInfo::exists(archivoCookies())) {
            tema::mostrarEstado(ui->etiquetaEstado, tr("No encuentro el archivo de cookies %1.").arg(archivoCookies()), tema::Estado::Error);
            return;
        }
        if (archivoCookies().isEmpty() && navegadorCookies().isEmpty()) {
            tema::mostrarEstado(ui->etiquetaEstado, tr("Para usar cookies hace falta LibreWolf o Firefox con la sesión iniciada (las de Chrome y Edge "
                                                       "no se pueden leer), o elegir un archivo cookies.txt."), tema::Estado::Error);
            return;
        }
    }
    if (!QDir().mkpath(destino())) {
        tema::mostrarEstado(ui->etiquetaEstado, tr("No se pudo crear la carpeta %1.").arg(destino()), tema::Estado::Error);
        return;
    }
    QDir(carpetaMiniaturas()).removeRecursively();
    QDir(carpetaSubtitulos()).removeRecursively();
    QDir().mkpath(carpetaMiniaturas());

    cola = urls;
    urlsPedidas = urls;
    eleccion = {};
    oferta.limpiar();
    total = urls.size();
    indice = listas = fallidas = 0;
    cancelado = false;
    ui->barraArchivo->setRange(0, 100);
    ui->barraArchivo->setValue(0);
    ui->barraTotal->setRange(0, total);
    ui->barraTotal->setValue(0);
    ui->barraArchivo->show();
    ui->barraTotal->show();
    habilitarFormulario(false);

    if (tipo == Tipo::Archivos) {
        siguienteArchivo();
        return;
    }
    if (remoto) {
        remotoEmpezar();
        return;
    }

    proceso.setProcessEnvironment(Herramientas::ambiente());

    // YouTube cambia seguido y un yt-dlp viejo deja de funcionar
    if (Herramientas::ytDlpDesactualizado()) {
        actualizando = true;
        tema::mostrarEstado(ui->etiquetaEstado, tr("Buscando actualizaciones de yt-dlp…"));
        proceso.start(Herramientas::programa(QStringLiteral("yt-dlp.exe")), {QStringLiteral("--update")});
        return;
    }
    empezar();
}

// Después de actualizar yt-dlp: en Videos, si se pidió, primero la consulta; si no, a descargar
void PaginaDescargas::empezar()
{
    actualizando = false;
    if (tipo == Tipo::Videos && ui->checkPistas->isChecked() && !cancelado) {
        consultando = true;
        indice = 0;
        ui->barraArchivo->hide();
        // La respuesta es JSON por la salida estándar: los avisos van aparte
        proceso.setProcessChannelMode(QProcess::SeparateChannels);
        siguienteConsulta();
        return;
    }
    siguienteUrl();
}

// «yt-dlp -J» de la URL siguiente (lo que hace Descargar.sh --pistas en Linux)
void PaginaDescargas::siguienteConsulta()
{
    if (cancelado) {
        consultando = false;
        proceso.setProcessChannelMode(QProcess::MergedChannels);
        finalizar();
        return;
    }
    const QString url = urlsPedidas.value(indice);
    ++indice;
    ui->barraTotal->setValue(indice - 1);
    tema::mostrarEstado(ui->etiquetaEstado, tr("Buscando calidades, subtítulos y pistas de audio (URL %1 de %2)…").arg(indice).arg(total));
    QStringList argumentos = Herramientas::argumentosYtDlp();
    argumentos << argumentosConsulta();
    argumentos << argumentosCookies();
    argumentos << QStringLiteral("--") << url;
    proceso.start(Herramientas::programa(QStringLiteral("yt-dlp.exe")), argumentos);
}

void PaginaDescargas::terminoConsulta(int codigo, QProcess::ExitStatus estado)
{
    const QByteArray respuestaJson = proceso.readAllStandardOutput();
    const QString errores = QString::fromUtf8(proceso.readAllStandardError());
    if (!cancelado && (codigo != 0 || estado != QProcess::NormalExit || !oferta.agregar(respuestaJson))) {
        static const QRegularExpression lineaError(QStringLiteral("^ERROR: (.*)$"), QRegularExpression::MultilineOption);
        QString mensaje;
        for (auto it = lineaError.globalMatch(errores); it.hasNext();)
            mensaje = it.next().captured(1).trimmed();
        anotar(tr("× No se pudo consultar %1: %2").arg(urlsPedidas.value(indice - 1),
                                                       mensaje.isEmpty() ? tr("yt-dlp terminó con código %1").arg(codigo) : mensaje));
        ++fallidas;
    }
    if (indice < total && !cancelado) {
        siguienteConsulta();
        return;
    }

    consultando = false;
    proceso.setProcessChannelMode(QProcess::MergedChannels);
    indice = 0;
    if (cancelado || fallidas == total) {
        // Si no se pudo consultar ninguna URL, tampoco se van a poder descargar
        cola.clear();
        finalizar();
        return;
    }
    fallidas = 0;
    if (oferta.vacia()) {
        anotar(tr("No hay calidades, subtítulos ni otras pistas de audio para elegir: se descarga como siempre."));
    } else {
        tema::mostrarEstado(ui->etiquetaEstado, tr("Elegí la calidad, los códecs y qué subtítulos y pistas de audio incluir."));
        DialogoPistas dialogo(oferta.audios.values(), oferta.subtitulos.values(), oferta.formatos, oferta.codecsAudio, this);
        if (dialogo.abrir() != QDialog::Accepted) {
            cancelado = true;
            cola.clear();
            finalizar();
            return;
        }
        eleccion = dialogo.eleccion();
    }
    ui->barraArchivo->show();
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
    tema::mostrarEstado(ui->etiquetaEstado, tr("URL %1 de %2: %3").arg(indice).arg(total).arg(url));

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
    if (musica()) {
        argumentos << QStringLiteral("-x");
        if (!ui->checkPlaylist->isChecked())
            argumentos << QStringLiteral("--no-playlist");
    } else {
        // MKV siempre; calidad, códecs, pistas y subtítulos si se eligieron
        argumentos << argumentosEleccion(eleccion, carpetaSubtitulos());
    }
    argumentos << argumentosCookies();
    argumentos << QStringLiteral("--") << url;

    proceso.start(Herramientas::programa(QStringLiteral("yt-dlp.exe")), argumentos);
}

void PaginaDescargas::cancelar()
{
    if (!ocupada())
        return;
    cancelado = true;
    cola.clear();
    tema::mostrarEstado(ui->etiquetaEstado, tr("Cancelando…"));
    if (tipo == Tipo::Archivos) {
        if (respuesta)
            respuesta->abort();    // su «terminó» sigue con finalizar()
        return;
    }
    if (remoto) {
        remotoCancelar();
        return;
    }
    Herramientas::cortar(proceso);
}

void PaginaDescargas::leerSalida()
{
    if (consultando)
        return;    // la respuesta de la consulta se lee entera al terminar
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
        tema::mostrarEstado(ui->etiquetaEstado, tr("Descargando %1").arg(archivoActual));
    } else if (linea.startsWith(QStringLiteral("[download]"))) {
        const QRegularExpressionMatch coincidencia = porcentaje.match(linea);
        if (coincidencia.hasMatch() && coincidencia.captured(1) != ultimoPorcentaje) {
            ultimoPorcentaje = coincidencia.captured(1);
            ui->barraArchivo->setValue(ultimoPorcentaje.toInt());
            if (ultimoPorcentaje.toInt() >= 100)
                tema::mostrarEstado(ui->etiquetaEstado, tr("Procesando %1 (metadatos y carátula)…").arg(archivoActual));
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
    // La miniatura está en la carpeta temporal (se vacía en cada descarga): se guarda una copia para la lista
    QString guardada;
    if (!miniatura.isEmpty()) {
        guardada = nuevaMiniatura();
        if (!QFile::copy(miniatura, guardada))
            guardada.clear();
    }
    // En Windows, yt-dlp termina cada línea con \r\n: el \r haría que el archivo no exista
    const QStringList lineas = QString::fromUtf8(archivo.readAll()).split(u'\n', Qt::SkipEmptyParts);
    for (const QString &linea : lineas) {
        const QString ruta = linea.trimmed();
        if (ruta.isEmpty())
            continue;
        const QFileInfo info(ruta);
        const QString nombre = musica() ? info.dir().dirName() + QStringLiteral(" — ") + info.completeBaseName()
                                        : info.completeBaseName();
        agregarDescargado(nombre, ruta, guardada);
        ++listas;
    }
}

void PaginaDescargas::terminoProceso(int codigo, QProcess::ExitStatus estado)
{
    if (consultando) {
        terminoConsulta(codigo, estado);
        return;
    }
    leerSalida();
    if (actualizando) {
        // Si no se pudo actualizar se sigue con la versión que hay
        if (codigo == 0 && estado == QProcess::NormalExit)
            Herramientas::marcarYtDlpActualizado();
        else if (!cancelado)
            anotar(tr("No se pudo actualizar yt-dlp; se usa la versión instalada."));
        empezar();
        return;
    }

    const int antes = listas;
    registrarTerminados(ultimaMiniatura());
    if (!cancelado && (codigo != 0 || estado != QProcess::NormalExit || listas == antes)) {
        ++fallidas;
        anotar(tr("× Falló: %1").arg(mensajeError.isEmpty() ? tr("yt-dlp terminó con código %1").arg(codigo) : mensajeError));
        if (musica() && !ui->checkCookies->isChecked())
            anotar(tr("   Puede deberse a la falta de cookies (probá con «Usar cookies» o «Bajar con el servidor»)."));
    }
    ui->barraTotal->setValue(indice);
    siguienteUrl();
}

// ── Descargas en masa: una URL por vez, a un archivo temporal propio en la misma carpeta ──

void PaginaDescargas::siguienteArchivo()
{
    if (cola.isEmpty() || cancelado) {
        finalizar();
        return;
    }
    const QString escrita = cola.takeFirst();
    ++indice;
    ponerProgreso(0, indice - 1);
    const QUrl url = QUrl::fromUserInput(escrita);
    if (!url.isValid() || (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https"))) {
        anotar(tr("× No es una dirección de internet: %1").arg(escrita));
        ++fallidas;
        siguienteArchivo();
        return;
    }
    archivoActual = descargas::nombreDesdeUrl(url, indice);
    // El temporal empieza con «.» y queda en la misma carpeta: al terminar solo se renombra
    delete parcial;
    parcial = new QTemporaryFile(QDir(destino()).filePath(QStringLiteral(".descarga-XXXXXX")), this);
    if (!parcial->open()) {
        anotar(tr("× No se pudo crear un archivo en %1.").arg(destino()));
        ++fallidas;
        siguienteArchivo();
        return;
    }
    tema::mostrarEstado(ui->etiquetaEstado, tr("Archivo %1 de %2: %3").arg(indice).arg(total).arg(archivoActual));
    if (!red)
        red = new QNetworkAccessManager(this);
    QNetworkRequest pedido(url);
    pedido.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Windows-Manager/%1").arg(QCoreApplication::applicationVersion()));
    respuesta = red->get(pedido);
    connect(respuesta, &QNetworkReply::readyRead, this, [this] {
        if (parcial && parcial->write(respuesta->readAll()) < 0)
            respuesta->abort();
    });
    connect(respuesta, &QNetworkReply::downloadProgress, this, [this](qint64 bajado, qint64 tamano) {
        ponerProgreso(tamano > 0 ? int(bajado * 100 / tamano) : -1, indice - 1);
    });
    connect(respuesta, &QNetworkReply::finished, this, &PaginaDescargas::terminoArchivo);
}

void PaginaDescargas::terminoArchivo()
{
    QNetworkReply *hecha = respuesta;
    respuesta = nullptr;
    hecha->deleteLater();
    if (parcial)
        parcial->write(hecha->readAll());

    bool guardado = false;
    if (hecha->error() == QNetworkReply::NoError && parcial && parcial->flush()) {
        // Recién ahora se elige el nombre final (otro programa pudo crear uno igual mientras bajaba);
        // rename nunca pisa: si justo apareció, se prueba con el siguiente
        const QDir carpeta(destino());
        for (int intento = 0; intento < 5 && !guardado; ++intento) {
            const QString final = carpeta.filePath(descargas::nombreLibre(carpeta, archivoActual));
            if (parcial->rename(final)) {
                parcial->setAutoRemove(false);    // rename no lo apaga: al borrar «parcial» se borraría el archivo ya guardado
                parcial->close();
                agregarDescargado(QFileInfo(final).fileName(), final, {});
                ++listas;
                guardado = true;
            }
        }
        if (!guardado) {
            anotar(tr("× No se pudo guardar %1 en %2.").arg(archivoActual, destino()));
            ++fallidas;
        }
    } else if (!cancelado) {
        anotar(tr("× No se pudo descargar %1: %2").arg(hecha->url().toString(), hecha->errorString()));
        ++fallidas;
    }
    // Si no se guardó, se borra solo el temporal propio (nunca un archivo que ya estaba)
    delete parcial;
    parcial = nullptr;
    ui->barraTotal->setValue(indice);
    siguienteArchivo();
}

void PaginaDescargas::finalizar()
{
    habilitarFormulario(true);
    ui->barraArchivo->setRange(0, 100);
    ui->barraTotal->setValue(ui->barraTotal->maximum());

    if (cancelado)
        tema::mostrarEstado(ui->etiquetaEstado, tr("Descarga cancelada."));
    else if (fallidas > 0)
        tema::mostrarEstado(ui->etiquetaEstado, tr("Terminado con errores: %1 descargado(s), %2 fallaron.").arg(listas).arg(fallidas), tema::Estado::Aviso);
    else if (tipo == Tipo::Archivos)
        tema::mostrarEstado(ui->etiquetaEstado, tr("Listo: %n archivo(s) descargado(s).", nullptr, listas), tema::Estado::Ok);
    else if (listas == 1)
        tema::mostrarEstado(ui->etiquetaEstado, musica() ? tr("Listo: 1 canción descargada.") : tr("Listo: 1 video descargado."), tema::Estado::Ok);
    else
        tema::mostrarEstado(ui->etiquetaEstado, (musica() ? tr("Listo: %1 canciones descargadas.") : tr("Listo: %1 videos descargados.")).arg(listas), tema::Estado::Ok);
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
        tema::mostrarEstado(ui->etiquetaEstado, tr("No se pudo abrir el archivo (error %1).").arg(resultado), tema::Estado::Error);
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
    if (archivo.moveToTrash() || !archivo.exists()) {
        const QString miniatura = item->data(RolMiniatura).toString();
        delete item;
        borrarMiniaturaSiSobra(miniatura);
        guardarDescargados();
    } else {
        anotar(tr("No se pudo mover a la papelera: %1").arg(archivo.fileName()));
    }
}
