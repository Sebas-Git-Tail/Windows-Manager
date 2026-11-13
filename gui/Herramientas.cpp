#include "Herramientas.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace Herramientas {

QString programa(const QString &nombre)
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("herramientas/") + nombre);
}

QStringList faltantes()
{
    QStringList lista;
    for (const QString &nombre : {QStringLiteral("yt-dlp.exe"), QStringLiteral("ffmpeg.exe"), QStringLiteral("ffprobe.exe"),
                                  QStringLiteral("qjs.exe"), QStringLiteral("rsync.exe"), QStringLiteral("ssh.exe"),
                                  QStringLiteral("askpass.exe")}) {
        if (!QFileInfo::exists(programa(nombre)))
            lista << nombre;
    }
    return lista;
}

QString datos()
{
    const QString carpeta = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(carpeta);
    return carpeta;
}

QString temporal()
{
    const QString carpeta = QDir(QDir::tempPath()).filePath(QStringLiteral("Windows-Manager"));
    QDir().mkpath(carpeta);
    return carpeta;
}

QProcessEnvironment ambiente()
{
    QProcessEnvironment variables = QProcessEnvironment::systemEnvironment();
    variables.insert(QStringLiteral("PATH"), QDir::toNativeSeparators(programa(QString())) + u';' + variables.value(QStringLiteral("PATH")));
    // Para que yt-dlp escriba los nombres en UTF-8 aunque su salida vaya a un programa
    variables.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    return variables;
}

QString rutaMsys(const QString &ruta)
{
    QString limpia = QDir::fromNativeSeparators(ruta);
    if (limpia.size() >= 2 && limpia.at(1) == u':')
        limpia = QStringLiteral("/cygdrive/") + limpia.left(1).toLower() + limpia.mid(2);
    return limpia;
}

QStringList argumentosYtDlp()
{
    return {QStringLiteral("--ffmpeg-location"), QDir::toNativeSeparators(programa(QString())),
            // Solo el incluido: si no, usaría el deno que haya instalado y variaría según la PC
            QStringLiteral("--no-js-runtimes"),
            QStringLiteral("--js-runtimes"), QStringLiteral("quickjs:") + QDir::toNativeSeparators(programa(QStringLiteral("qjs.exe"))),
            QStringLiteral("--cache-dir"), QDir::toNativeSeparators(QDir(temporal()).filePath(QStringLiteral("yt-dlp"))),
            QStringLiteral("--encoding"), QStringLiteral("utf-8")};
}

static QString marcaActualizacion()
{
    return QDir(datos()).filePath(QStringLiteral("yt-dlp-actualizado"));
}

bool ytDlpDesactualizado()
{
    const QFileInfo marca(marcaActualizacion());
    return !marca.exists() || marca.lastModified().secsTo(QDateTime::currentDateTime()) > 24 * 60 * 60;
}

void marcarYtDlpActualizado()
{
    QFile archivo(marcaActualizacion());
    if (archivo.open(QIODevice::WriteOnly))
        archivo.write(QDateTime::currentDateTime().toString(Qt::ISODate).toUtf8());
}

void cortar(QProcess &proceso)
{
    if (proceso.state() == QProcess::NotRunning)
        return;
    QProcess::execute(QStringLiteral("taskkill"), {QStringLiteral("/PID"), QString::number(proceso.processId()),
                                                   QStringLiteral("/T"), QStringLiteral("/F")});
}

}
