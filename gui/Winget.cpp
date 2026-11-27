#include "Winget.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace winget {

QString limpiar(const QByteArray &salida)
{
    static const QRegularExpression color(QStringLiteral("\\x1b\\[[0-9;?]*[A-Za-z]"));
    static const QRegularExpression giro(QStringLiteral("^\\s*[\\\\|/-]\\s*$"));    // un solo carácter
    QString texto = QString::fromUtf8(salida);
    texto.remove(color);
    QStringList lineas;
    const QStringList crudas = texto.split(u'\n');
    for (const QString &cruda : crudas) {
        // Lo que se reescribió con \r: queda lo último
        QString linea = cruda;
        while (linea.endsWith(u'\r'))
            linea.chop(1);
        const qsizetype retorno = linea.lastIndexOf(u'\r');
        if (retorno >= 0)
            linea = linea.mid(retorno + 1);
        // El giro de espera (- \ | /) no es texto
        if (!linea.trimmed().isEmpty() && giro.match(linea).hasMatch())
            continue;
        while (linea.endsWith(u' '))
            linea.chop(1);
        lineas << linea;
    }
    return lineas.join(u'\n');
}

bool esProgreso(const QString &linea)
{
    static const QRegularExpression porcentaje(QStringLiteral("^\\s*\\d{1,3}%\\s*$"));
    return linea.contains(QChar(0x2588)) || linea.contains(QChar(0x2592)) || porcentaje.match(linea).hasMatch();
}

QList<Paquete> leerTabla(const QString &texto)
{
    static const QRegularExpression guiones(QStringLiteral("^-{5,}$"));
    // «3 actualizaciones disponibles.», «2 upgrades available.», «1 paquete tiene…»
    static const QRegularExpression resumen(QStringLiteral("^\\d+\\s+\\S+.*(disponible|available|tiene|have|has)"),
                                            QRegularExpression::CaseInsensitiveOption);
    const QStringList lineas = texto.split(u'\n');
    qsizetype separador = -1;
    for (qsizetype i = 1; i < lineas.size(); ++i) {
        if (guiones.match(lineas.at(i).trimmed()).hasMatch() && !lineas.at(i - 1).trimmed().isEmpty()) {
            separador = i;
            break;
        }
    }
    if (separador < 0)
        return {};

    // Dónde empieza cada columna: cada palabra del encabezado
    const QString encabezado = lineas.at(separador - 1);
    QList<qsizetype> inicios;
    QStringList nombres;
    for (qsizetype p = 0; p < encabezado.size(); ++p) {
        if (encabezado.at(p) != u' ' && (p == 0 || encabezado.at(p - 1) == u' ')) {
            inicios << p;
            const qsizetype fin = encabezado.indexOf(u' ', p);
            nombres << encabezado.mid(p, fin < 0 ? -1 : fin - p).toLower();
        }
    }
    if (inicios.size() < 3)
        return {};
    int columnaDisponible = -1, columnaOrigen = -1;
    for (int c = 3; c < nombres.size(); ++c) {
        if (nombres.at(c).startsWith(QStringLiteral("disp")) || nombres.at(c).startsWith(QStringLiteral("avail")))
            columnaDisponible = c;
        else if (nombres.at(c).startsWith(QStringLiteral("orig")) || nombres.at(c).startsWith(QStringLiteral("sour")))
            columnaOrigen = c;
    }

    QList<Paquete> paquetes;
    for (qsizetype i = separador + 1; i < lineas.size(); ++i) {
        const QString &linea = lineas.at(i);
        if (linea.trimmed().isEmpty() || resumen.match(linea.trimmed()).hasMatch())
            break;
        const auto campo = [&](int c) {
            if (c < 0 || c >= inicios.size() || inicios.at(c) >= linea.size())
                return QString();
            const qsizetype largo = c + 1 < inicios.size() ? inicios.at(c + 1) - inicios.at(c) : -1;
            return linea.mid(inicios.at(c), largo).trimmed();
        };
        Paquete paquete{campo(0), campo(1), campo(2), campo(columnaDisponible), campo(columnaOrigen)};
        if (paquete.id.isEmpty() || paquete.id.contains(u' '))    // no es una fila de la tabla
            continue;
        paquetes << paquete;
    }
    return paquetes;
}

namespace {
const QString Acuerdos = QStringLiteral("--accept-source-agreements");
const QString AcuerdosPaquete = QStringLiteral("--accept-package-agreements");
}

QStringList argumentosBuscar(const QString &texto)
{
    return {QStringLiteral("search"), QStringLiteral("--query"), texto, Acuerdos};
}

QStringList argumentosInstalar(const QString &id)
{
    return {QStringLiteral("install"), QStringLiteral("--id"), id, QStringLiteral("--exact"), Acuerdos, AcuerdosPaquete};
}

QStringList argumentosActualizables()
{
    return {QStringLiteral("upgrade"), Acuerdos};
}

QStringList argumentosActualizar(const QString &id)
{
    if (id.isEmpty())
        return {QStringLiteral("upgrade"), QStringLiteral("--all"), Acuerdos, AcuerdosPaquete};
    return {QStringLiteral("upgrade"), QStringLiteral("--id"), id, QStringLiteral("--exact"), Acuerdos, AcuerdosPaquete};
}

QString mensajeCodigo(int codigo)
{
    if (codigo == 0)
        return {};
    const auto hresult = quint32(codigo);
    switch (hresult) {
    case 0x8A150014u:
        return QCoreApplication::translate("winget", "No se encontró ningún programa con ese nombre.");
    case 0x8A15002Bu:
        return QCoreApplication::translate("winget", "No hay una versión más nueva para instalar.");
    default:
        break;
    }
    return QCoreApplication::translate("winget", "winget terminó con el código 0x%1.").arg(hresult, 8, 16, QLatin1Char('0'));
}

}
