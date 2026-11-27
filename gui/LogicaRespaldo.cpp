#include "LogicaRespaldo.h"

#include <QRegularExpression>
#include <algorithm>

namespace respaldo {

QString normalizarPersona(const QString &escrito)
{
    static const QRegularExpression valida(QStringLiteral("^[a-z0-9][a-z0-9._-]*$"));
    const QString nombre = escrito.trimmed().toLower();    // las carpetas van en minúsculas
    return valida.match(nombre).hasMatch() ? nombre : QString();
}

QString validarComputadora(const QString &escrito)
{
    static const QRegularExpression valido(QStringLiteral("^[^/\\\\.\\p{Cc}][^/\\\\\\p{Cc}]*$"));
    const QString nombre = escrito.trimmed();
    if (!valido.match(nombre).hasMatch())
        return {};
    // «PC.anteriores» sería la carpeta de versiones anteriores de la computadora «PC»
    if (nombre.endsWith(QStringLiteral(".anteriores"), Qt::CaseInsensitive))
        return {};
    return nombre;
}

// Lo que no se copia nunca (rutas desde la carpeta personal). AppData es muy pesada y sus programas la
// tienen abierta; NTUSER es el registro, que Windows tiene bloqueado; OneDrive traería todo lo que esté
// solo en la nube. El resto es lo mismo que excluye Rsync_Filtro.conf en Linux.
const QStringList &exclusionesBase()
{
    static const QStringList lista{
        QStringLiteral("/AppData/"), QStringLiteral("/NTUSER.DAT*"), QStringLiteral("/ntuser.*"), QStringLiteral("/OneDrive*/"),
        QStringLiteral("/.ssh/"), QStringLiteral("/.cache/"), QStringLiteral("/.java/"), QStringLiteral("/Games/"),
        QStringLiteral("/VirtualBox VMs/"), QStringLiteral("/.npm/"), QStringLiteral("/.ollama/")};
    return lista;
}

QString rutaRelativaSegura(const QString &escrita, bool *ok)
{
    static const QRegularExpression control(QStringLiteral("\\p{Cc}"));
    if (ok)
        *ok = false;
    QString ruta = escrita.trimmed();
    ruta.replace(u'\\', u'/');
    if (ruta.contains(control) || ruta.contains(u':'))    // «C:» sería otra unidad (o un servidor, para rsync)
        return {};
    QStringList partes;
    const QStringList trozos = ruta.split(u'/', Qt::SkipEmptyParts);
    for (const QString &trozo : trozos) {
        const QString parte = trozo.trimmed();
        if (parte == QStringLiteral(".."))
            return {};
        if (parte.isEmpty() || parte == QStringLiteral("."))
            continue;
        partes << trozo;
    }
    if (ok)
        *ok = true;
    return partes.join(u'/');
}

QString normalizarExclusion(const QString &relativa)
{
    bool ok = false;
    QString ruta = rutaRelativaSegura(relativa, &ok);
    if (!ok || ruta.isEmpty())
        return {};
    // Para rsync, * ? y [ son comodines: un nombre que los tenga va escapado
    QString escapada;
    for (const QChar c : std::as_const(ruta)) {
        if (c == u'*' || c == u'?' || c == u'[')
            escapada += u'\\';
        escapada += c;
    }
    return u'/' + escapada + u'/';
}

QStringList exclusiones(const QStringList &extras)
{
    QStringList lista = exclusionesBase();
    for (const QString &extra : extras) {
        const QString normal = normalizarExclusion(extra);
        if (!normal.isEmpty() && !lista.contains(normal))
            lista << normal;
    }
    return lista;
}

QString nombreFecha(const QDateTime &momento)
{
    return momento.toString(QStringLiteral("yyyy-MM-dd_HH-mm"));
}

QDateTime fechaDeNombre(const QString &nombre)
{
    static const QRegularExpression forma(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}_\\d{2}-\\d{2}$"));
    if (!forma.match(nombre).hasMatch())
        return {};
    return QDateTime::fromString(nombre, QStringLiteral("yyyy-MM-dd_HH-mm"));
}

QString carpetaAnteriores(const QString &rutaDestino)
{
    QString ruta = rutaDestino;
    while (ruta.endsWith(u'/'))
        ruta.chop(1);
    return ruta + QStringLiteral(".anteriores/");
}

QString backupDirRelativo(const QString &computadora, const QString &fecha)
{
    return QStringLiteral("../%1.anteriores/%2").arg(computadora, fecha);
}

QStringList fechasDeListado(const QString &listado)
{
    // «drwxr-xr-x          4,096 2026/11/26 10:00:00 2026-11-26_10-00»
    static const QRegularExpression linea(QStringLiteral("^d\\S*\\s+[\\d,.]+\\s+\\S+\\s+\\S+\\s+(\\d{4}-\\d{2}-\\d{2}_\\d{2}-\\d{2})\\s*$"),
                                          QRegularExpression::MultilineOption);
    QStringList fechas;
    for (auto it = linea.globalMatch(listado); it.hasNext();) {
        const QString nombre = it.next().captured(1);
        if (fechaDeNombre(nombre).isValid() && !fechas.contains(nombre))
            fechas << nombre;
    }
    std::sort(fechas.begin(), fechas.end(), std::greater<QString>());    // el formato ordena como la fecha
    return fechas;
}

QStringList anterioresViejas(const QStringList &fechas, const QDateTime &ahora, int dias)
{
    const QDateTime limite = ahora.addDays(-dias);
    QStringList viejas;
    for (const QString &nombre : fechas) {
        const QDateTime fecha = fechaDeNombre(nombre);
        if (fecha.isValid() && fecha < limite)
            viejas << nombre;
    }
    return viejas;
}

// Huella fija: solo se conecta si del otro lado está el servidor de verdad (las huellas vienen compiladas
// en el programa, ConfigServidor::huellasSsh). HostKeyAlias: la huella vale aunque cambie la IP. Una sola
// contraseña por intento.
QString comandoSsh(const QString &ssh, const QString &huellas, int puerto)
{
    return QStringLiteral("\"%1\" -F none -p %3 -o StrictHostKeyChecking=yes -o HostKeyAlias=servidor-respaldos "
                          "-o UpdateHostKeys=no -o UserKnownHostsFile=\"%2\" "
                          "-o PubkeyAuthentication=no -o PreferredAuthentications=password,keyboard-interactive "
                          "-o NumberOfPasswordPrompts=1 -o ConnectTimeout=10")
        .arg(ssh, huellas).arg(puerto);
}

QStringList argumentosRespaldo(const OpcionesRespaldo &o)
{
    // Sin -a: los permisos de Windows no significan nada en el servidor. Por eso -r -t y --chmod;
    // sin -l, porque las carpetas de sistema de Windows son enlaces que dan vueltas
    QStringList argumentos{QStringLiteral("-rth"), QStringLiteral("--chmod=D755,F644"),
                           o.backupDir.isEmpty() ? QStringLiteral("--info=progress2,name1")
                                                 : QStringLiteral("--info=progress2,name1,backup1"),
                           // sin esto, rsync escribe las tildes de los nombres como códigos
                           QStringLiteral("-8"),
                           QStringLiteral("-e"), o.ssh,
                           // la ruta puede tener espacios (el nombre de la computadora)
                           QStringLiteral("--protect-args"),
                           // la carpeta de la persona puede no existir todavía
                           QStringLiteral("--mkpath"),
                           QStringLiteral("--delete")};
    // Lo que se borra o se pisa en el servidor va a la carpeta de versiones anteriores
    if (!o.backupDir.isEmpty())
        argumentos << QStringLiteral("--backup") << QStringLiteral("--backup-dir=") + o.backupDir;
    for (const QString &excluido : o.excluidos)
        argumentos << QStringLiteral("--exclude") << excluido;
    if (o.simular) {
        argumentos << QStringLiteral("--dry-run") << QStringLiteral("--itemize-changes") << QStringLiteral("--stats")
                   << QStringLiteral("--info=del1");
    }
    argumentos << o.origen << o.destino;
    return argumentos;
}

QStringList argumentosRestaurar(const OpcionesRestaurar &o)
{
    QStringList argumentos{QStringLiteral("-rth"), QStringLiteral("--info=progress2,name1"), QStringLiteral("-8"),
                           QStringLiteral("-e"), o.ssh, QStringLiteral("--protect-args"), QStringLiteral("--mkpath")};
    // Nunca --delete: restaurar no borra nada de la computadora
    if (!o.reemplazar)
        argumentos << QStringLiteral("--ignore-existing");
    else if (!o.reemplazados.isEmpty())
        argumentos << QStringLiteral("--backup") << QStringLiteral("--backup-dir=") + o.reemplazados;
    if (o.simular)
        argumentos << QStringLiteral("--dry-run") << QStringLiteral("--itemize-changes") << QStringLiteral("--stats");

    QString base = o.base;
    if (!base.endsWith(u'/'))
        base += u'/';
    const QString personal = o.carpetaPersonal.endsWith(u'/') ? o.carpetaPersonal : o.carpetaPersonal + u'/';
    if (o.relativa.isEmpty()) {
        argumentos << QStringLiteral("%1:%2").arg(o.servidor, base) << personal;
    } else {
        // «Documentos/Fotos» se trae a la carpeta Documentos (sin / al final: la carpeta entera, o el archivo)
        const qsizetype barra = o.relativa.lastIndexOf(u'/');
        const QString padre = barra < 0 ? QString() : o.relativa.left(barra) + u'/';
        argumentos << QStringLiteral("%1:%2%3").arg(o.servidor, base, o.relativa) << personal + padre;
    }
    return argumentos;
}

QStringList argumentosListar(const QString &ssh, const QString &remoto)
{
    return {QStringLiteral("-e"), ssh, QStringLiteral("--protect-args"), QStringLiteral("--list-only"), remoto};
}

QStringList argumentosLimpiar(const QString &ssh, const QString &vacia, const QString &remoto, const QStringList &viejas)
{
    QStringList argumentos{QStringLiteral("-r"), QStringLiteral("--delete"), QStringLiteral("-e"), ssh,
                           QStringLiteral("--protect-args")};
    for (const QString &vieja : viejas) {
        if (fechaDeNombre(vieja).isValid())    // solo nombres de fecha: nunca un comodín
            argumentos << QStringLiteral("--include=/%1/***").arg(vieja);
    }
    argumentos << QStringLiteral("--exclude=*");
    argumentos << (vacia.endsWith(u'/') ? vacia : vacia + u'/') << remoto;
    return argumentos;
}

LectorRsync::Linea LectorRsync::procesar(const QString &texto)
{
    // «2.00MB  99%  2.00GB/s ...»: anclada, porque un archivo puede llamarse «99% seguro.txt»
    static const QRegularExpression progreso(QStringLiteral("^\\S+\\s+(\\d+)%\\s"));
    const QString linea = texto.trimmed();
    Linea resultado;
    resultado.texto = linea;
    if (linea.isEmpty())
        return resultado;

    const QRegularExpressionMatch coincidencia = progreso.match(linea);
    if (coincidencia.hasMatch()) {
        resultado.tipo = Tipo::Progreso;
        resultado.porcentaje = coincidencia.captured(1).toInt();
        return resultado;
    }
    listado_ << linea;
    if (linea.startsWith(QStringLiteral("backed up "))) {
        ++respaldados_;
        resultado.tipo = Tipo::Respaldado;
    } else if (linea.startsWith(QStringLiteral("(new) backup_dir"))) {
        // rsync avisa que creó la carpeta de versiones anteriores
    } else if (linea.contains(QStringLiteral("Permission denied")) || linea.startsWith(QStringLiteral("ssh:"))
               || linea.contains(QStringLiteral("Connection closed")) || linea.contains(QStringLiteral("Host key verification"))) {
        errorConexion_ = linea;
        resultado.tipo = Tipo::Aviso;
    } else if (linea.startsWith(QStringLiteral("IO error"))) {
        resultado.tipo = Tipo::Aviso;
    } else if (linea.startsWith(QStringLiteral("rsync"))) {
        if (linea.contains(QStringLiteral("(0 bytes received so far)")))
            sinDatos_ = true;
        resultado.tipo = Tipo::Aviso;
    } else if (linea != QStringLiteral("sending incremental file list") && linea != QStringLiteral("receiving incremental file list")
               && !linea.startsWith(QStringLiteral("sent ")) && !linea.startsWith(QStringLiteral("total size"))
               && linea != QStringLiteral("./")) {
        resultado.tipo = Tipo::Archivo;
    }
    return resultado;
}

bool LectorRsync::huellaDistinta() const
{
    return errorConexion_.contains(QStringLiteral("Host key verification"))
           || errorConexion_.contains(QStringLiteral("host key"), Qt::CaseInsensitive);
}

bool LectorRsync::contrasenaMala() const
{
    return sinDatos_ && errorConexion_.contains(QStringLiteral("Permission denied"));
}

Final clasificar(int codigo, QProcess::ExitStatus estado, bool cancelado, const LectorRsync &lector)
{
    if (cancelado)
        return Final::Cancelado;
    if (estado == QProcess::CrashExit)    // se cortó solo (o lo cortó Windows): no es que se haya cancelado
        return Final::Fallo;
    if (codigo == 0)
        return Final::Ok;
    if (codigo == CodigoIncompleto || codigo == CodigoDesaparecido)
        return Final::Incompleto;
    if (lector.huellaDistinta())
        return Final::HuellaDistinta;
    if (lector.contrasenaMala())
        return Final::ContrasenaMala;
    return Final::Error;
}

}
