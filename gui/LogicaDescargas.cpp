#include "LogicaDescargas.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace descargas {

QStringList urlsDeTexto(const QString &texto)
{
    QStringList urls;
    const QStringList lineas = texto.split(u'\n');
    for (const QString &linea : lineas) {
        if (!linea.trimmed().isEmpty())
            urls << linea.trimmed();
    }
    return urls;
}

QString nombreSeguro(const QString &original)
{
    static const QRegularExpression prohibidos(QStringLiteral("[\\x00-\\x1f\"*:<>?\\\\|/]"));
    static const QRegularExpression reservado(QStringLiteral("^(con|prn|aux|nul|com[1-9]|lpt[1-9])(\\..*)?$"),
                                              QRegularExpression::CaseInsensitiveOption);
    QString nombre = original;
    nombre.replace(prohibidos, QStringLiteral("_"));
    nombre = nombre.trimmed();
    while (nombre.endsWith(u'.') || nombre.endsWith(u' '))    // Windows los saca solo y el nombre cambiaría
        nombre.chop(1);
    if (nombre.isEmpty())
        return QStringLiteral("_");
    if (reservado.match(nombre).hasMatch())
        nombre.prepend(u'_');
    return nombre;
}

QString nombreDesdeUrl(const QUrl &url, int numero)
{
    // Se corta por las barras del camino codificado: una «%2F» es parte del nombre, no otra carpeta
    QString camino = url.path(QUrl::FullyEncoded);
    while (camino.endsWith(u'/'))
        camino.chop(1);
    const QString ultimo = QUrl::fromPercentEncoding(camino.section(u'/', -1).toUtf8());
    if (ultimo.isEmpty() || ultimo == QStringLiteral(".") || ultimo == QStringLiteral(".."))
        return QStringLiteral("descarga-%1").arg(numero);
    return nombreSeguro(ultimo);
}

QString nombreLibre(const QDir &carpeta, const QString &nombre)
{
    const auto ocupado = [&](const QString &n) {
        const QFileInfo info(carpeta.filePath(n));
        return info.exists() || info.isSymLink();
    };
    if (!ocupado(nombre))
        return nombre;
    static const QRegularExpression doble(QStringLiteral("^(.+)(\\.tar\\.[A-Za-z0-9]+)$"));
    static const QRegularExpression simple(QStringLiteral("^(.+)(\\.[^.]+)$"));
    QString base = nombre, extension;
    QRegularExpressionMatch m = doble.match(nombre);
    if (!m.hasMatch())
        m = simple.match(nombre);
    if (m.hasMatch()) {
        base = m.captured(1);
        extension = m.captured(2);
    }
    for (int i = 2;; ++i) {
        const QString candidato = QStringLiteral("%1 (%2)%3").arg(base, QString::number(i), extension);
        if (!ocupado(candidato))
            return candidato;
    }
}

}
