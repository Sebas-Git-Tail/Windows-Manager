// Lógica del respaldo sin ventana (se prueba en Linux, tests/): validar los nombres, armar los
// argumentos de rsync (respaldo, restauración, versiones anteriores) y leer lo que escribe rsync.
//
// Versiones anteriores: el respaldo sigue siendo una copia espejo (--delete), pero lo que rsync borra o
// pisa en el servidor no se pierde: va a una carpeta hermana del destino,
//   Respaldos/<persona>/<computadora>.anteriores/<AAAA-MM-DD_HH-MM>/
// (--backup --backup-dir=../<computadora>.anteriores/<fecha>, relativo al destino). Queda fuera de lo que
// limpia --delete. Las de más de 30 días se borran al terminar, con rsync (sin dar permisos nuevos).

#pragma once

#include <QDateTime>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace respaldo {

constexpr int PuertoSsh = 22;    // servidor.json no trae el puerto (si algún día lo trae, leerlo de ConfigServidor)
constexpr int DiasAnteriores = 30;
constexpr int CodigoIncompleto = 23;    // rsync: archivos que no se pudieron leer
constexpr int CodigoDesaparecido = 24;  // rsync: archivos que se borraron durante la copia

// El nombre de la persona es una carpeta del servidor: minúsculas, sin barras ni «..». Vacío si no sirve.
QString normalizarPersona(const QString &escrito);
// Admite espacios y mayúsculas, pero no puede salirse de la carpeta de la persona ni llamarse como la
// carpeta de versiones anteriores de otra computadora («… .anteriores»). Vacío si no sirve.
QString validarComputadora(const QString &escrito);

// Exclusiones: una lista base fija y las carpetas que agrega el usuario (relativas a la carpeta personal)
const QStringList &exclusionesBase();
QString normalizarExclusion(const QString &relativa);   // «Documentos\\Juegos» → «/Documentos/Juegos/»; vacío si no sirve
QStringList exclusiones(const QStringList &extras);     // base + extras normalizadas, sin repetir

// Una ruta relativa a la carpeta personal (lo que se restaura): barras normalizadas, sin «..» ni «.».
// Vacío = todo. ok queda en falso si la ruta no sirve.
QString rutaRelativaSegura(const QString &escrita, bool *ok = nullptr);

// Versiones anteriores
QString nombreFecha(const QDateTime &momento);                     // AAAA-MM-DD_HH-MM
QString carpetaAnteriores(const QString &rutaDestino);             // «…/ana/PC/» → «…/ana/PC.anteriores/»
QString backupDirRelativo(const QString &computadora, const QString &fecha);   // ../PC.anteriores/fecha
QStringList fechasDeListado(const QString &listado);               // salida de rsync --list-only, más nuevas primero
QStringList anterioresViejas(const QStringList &fechas, const QDateTime &ahora, int dias = DiasAnteriores);
QDateTime fechaDeNombre(const QString &nombre);                    // inválida si no es AAAA-MM-DD_HH-MM

// Comando de ssh para «rsync -e»: huella fija, solo contraseña, una por intento
QString comandoSsh(const QString &ssh, const QString &huellas, int puerto = PuertoSsh);

struct OpcionesRespaldo {
    QString ssh;            // lo que va en -e
    QString origen;         // carpeta personal, con / al final (formato MSYS)
    QString destino;        // usuario@servidor:ruta/
    QStringList excluidos;
    QString backupDir;      // vacío: sin versiones anteriores (la simulación)
    bool simular = false;
};
QStringList argumentosRespaldo(const OpcionesRespaldo &o);

struct OpcionesRestaurar {
    QString ssh;
    QString servidor;       // usuario@servidor
    QString base;           // carpeta del respaldo en el servidor (la última o una versión anterior), con /
    QString relativa;       // qué restaurar (vacío: todo)
    QString carpetaPersonal;   // formato MSYS, sin / al final
    bool reemplazar = false;   // sin esto no se pisa nada de la computadora (--ignore-existing)
    QString reemplazados;      // si se reemplaza, ahí queda lo que había (formato MSYS); vacío: no se guarda
    bool simular = false;
};
QStringList argumentosRestaurar(const OpcionesRestaurar &o);

// rsync --list-only de la carpeta de versiones anteriores
QStringList argumentosListar(const QString &ssh, const QString &remoto);
// Borra del servidor solo las carpetas «viejas» de .anteriores: copia una carpeta vacía con --delete y
// filtros que incluyen solo esas fechas (lo excluido nunca se borra)
QStringList argumentosLimpiar(const QString &ssh, const QString &vacia, const QString &remoto, const QStringList &viejas);

// Lee la salida de rsync línea por línea (rsync separa el progreso con \r y lo demás con \n)
class LectorRsync
{
public:
    enum class Tipo { Nada, Progreso, Aviso, Archivo, Respaldado };
    struct Linea {
        Tipo tipo = Tipo::Nada;
        int porcentaje = -1;
        QString texto;
    };
    Linea procesar(const QString &linea);

    bool sinDatos() const { return sinDatos_; }           // rsync no llegó a recibir nada del servidor
    QString errorConexion() const { return errorConexion_; }
    bool huellaDistinta() const;
    bool contrasenaMala() const;
    int respaldados() const { return respaldados_; }       // archivos que fueron a «anteriores»
    QStringList listado() const { return listado_; }      // líneas sin clasificar (para --list-only)

private:
    bool sinDatos_ = false;
    QString errorConexion_;
    int respaldados_ = 0;
    QStringList listado_;
};

// Cómo terminó rsync
enum class Final { Ok, Incompleto, Cancelado, Fallo, HuellaDistinta, ContrasenaMala, Error };
Final clasificar(int codigo, QProcess::ExitStatus estado, bool cancelado, const LectorRsync &lector);

}
