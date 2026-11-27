// Respaldo: nombres, exclusiones, argumentos de rsync (respaldo, restauración y versiones anteriores),
// rutaMsys y lectura de la salida de rsync.

#include "Herramientas.h"
#include "LogicaRespaldo.h"

#include <QtTest>

using namespace respaldo;

class PruebaRespaldo : public QObject
{
    Q_OBJECT

private slots:
    void persona()
    {
        QCOMPARE(normalizarPersona(QStringLiteral("  Ana ")), QStringLiteral("ana"));
        QCOMPARE(normalizarPersona(QStringLiteral("juan.perez-2")), QStringLiteral("juan.perez-2"));
        QVERIFY(normalizarPersona(QStringLiteral("..")).isEmpty());
        QVERIFY(normalizarPersona(QStringLiteral("ana/../x")).isEmpty());
        QVERIFY(normalizarPersona(QStringLiteral("ana perez")).isEmpty());
        QVERIFY(normalizarPersona(QStringLiteral(".oculta")).isEmpty());
        QVERIFY(normalizarPersona(QString()).isEmpty());
    }

    void computadora()
    {
        QCOMPARE(validarComputadora(QStringLiteral(" PC de Ana ")), QStringLiteral("PC de Ana"));
        QVERIFY(validarComputadora(QStringLiteral("a/b")).isEmpty());
        QVERIFY(validarComputadora(QStringLiteral("a\\b")).isEmpty());
        QVERIFY(validarComputadora(QStringLiteral("..")).isEmpty());
        QVERIFY(validarComputadora(QStringLiteral(".x")).isEmpty());
        QVERIFY(validarComputadora(QStringLiteral("PC\nX")).isEmpty());
        // sería la carpeta de versiones anteriores de otra computadora
        QVERIFY(validarComputadora(QStringLiteral("PC.anteriores")).isEmpty());
        QCOMPARE(validarComputadora(QStringLiteral("PC.v2")), QStringLiteral("PC.v2"));
    }

    void rutaMsys()
    {
        QCOMPARE(Herramientas::rutaMsys(QStringLiteral("C:\\Users\\Ana")), QStringLiteral("/cygdrive/c/Users/Ana"));
        QCOMPARE(Herramientas::rutaMsys(QStringLiteral("D:/Datos/x y")), QStringLiteral("/cygdrive/d/Datos/x y"));
        QCOMPARE(Herramientas::rutaMsys(QStringLiteral("/ya/es/unix")), QStringLiteral("/ya/es/unix"));
    }

    void rutaRelativa()
    {
        bool ok = false;
        QCOMPARE(rutaRelativaSegura(QStringLiteral("Documentos\\Fotos\\"), &ok), QStringLiteral("Documentos/Fotos"));
        QVERIFY(ok);
        QCOMPARE(rutaRelativaSegura(QStringLiteral("./a//b/./c.txt"), &ok), QStringLiteral("a/b/c.txt"));
        QVERIFY(ok);
        QCOMPARE(rutaRelativaSegura(QStringLiteral("  "), &ok), QString());
        QVERIFY(ok);    // vacío = todo
        rutaRelativaSegura(QStringLiteral("a/../../b"), &ok);
        QVERIFY(!ok);
        rutaRelativaSegura(QStringLiteral("C:\\Windows"), &ok);
        QVERIFY(!ok);
    }

    void exclusiones_()
    {
        QCOMPARE(normalizarExclusion(QStringLiteral("Juegos")), QStringLiteral("/Juegos/"));
        QCOMPARE(normalizarExclusion(QStringLiteral("Documentos\\Mis juegos\\")), QStringLiteral("/Documentos/Mis juegos/"));
        QCOMPARE(normalizarExclusion(QStringLiteral("raro[1]*")), QStringLiteral("/raro\\[1]\\*/"));
        QVERIFY(normalizarExclusion(QStringLiteral("..")).isEmpty());
        QVERIFY(normalizarExclusion(QString()).isEmpty());
        const QStringList lista = exclusiones({QStringLiteral("Juegos"), QStringLiteral("Juegos/"), QStringLiteral("AppData"), QStringLiteral("../x")});
        QVERIFY(lista.startsWith(exclusionesBase().first()));
        QCOMPARE(lista.size(), exclusionesBase().size() + 1);    // AppData ya estaba; ../x no sirve
        QCOMPARE(lista.last(), QStringLiteral("/Juegos/"));
    }

    void fechas()
    {
        const QDateTime momento(QDate(2026, 11, 26), QTime(9, 5));
        QCOMPARE(nombreFecha(momento), QStringLiteral("2026-11-26_09-05"));
        QCOMPARE(fechaDeNombre(QStringLiteral("2026-11-26_09-05")), momento);
        QVERIFY(!fechaDeNombre(QStringLiteral("2026-11-26")).isValid());
        QVERIFY(!fechaDeNombre(QStringLiteral("*")).isValid());
        QCOMPARE(carpetaAnteriores(QStringLiteral("/home/s/Respaldos/ana/PC de Ana/")), QStringLiteral("/home/s/Respaldos/ana/PC de Ana.anteriores/"));
        QCOMPARE(backupDirRelativo(QStringLiteral("PC de Ana"), QStringLiteral("2026-11-26_09-05")),
                 QStringLiteral("../PC de Ana.anteriores/2026-11-26_09-05"));

        const QString listado = QStringLiteral(
            "drwxr-xr-x             32 2026/11/26 15:44:14 .\n"
            "drwxr-xr-x             26 2026/10/01 10:00:00 2026-10-01_10-00\n"
            "drwxr-xr-x          4,096 2026/11/26 15:44:14 2026-11-26_15-44\n"
            "-rw-r--r--              3 2026/11/26 15:44:14 2026-11-20_10-00\n"    // un archivo, no cuenta
            "drwxr-xr-x             26 2026/11/26 15:44:14 otra cosa\n");
        const QStringList lista = fechasDeListado(listado);
        QCOMPARE(lista, (QStringList{QStringLiteral("2026-11-26_15-44"), QStringLiteral("2026-10-01_10-00")}));
        QCOMPARE(anterioresViejas(lista, QDateTime(QDate(2026, 11, 26), QTime(16, 0))), QStringList{QStringLiteral("2026-10-01_10-00")});
        QVERIFY(anterioresViejas(lista, QDateTime(QDate(2026, 10, 20), QTime(0, 0))).isEmpty());
    }

    void argumentosDeRespaldo()
    {
        OpcionesRespaldo o;
        o.ssh = QStringLiteral("ssh");
        o.origen = QStringLiteral("/cygdrive/c/Users/Ana/");
        o.destino = QStringLiteral("s@1.2.3.4:/R/ana/PC/");
        o.excluidos = exclusiones({});
        o.backupDir = backupDirRelativo(QStringLiteral("PC"), QStringLiteral("2026-11-26_09-05"));
        const QStringList a = argumentosRespaldo(o);
        QVERIFY(a.contains(QStringLiteral("--delete")));
        QVERIFY(a.contains(QStringLiteral("--backup")));
        QVERIFY(a.contains(QStringLiteral("--backup-dir=../PC.anteriores/2026-11-26_09-05")));
        QVERIFY(a.contains(QStringLiteral("--info=progress2,name1,backup1")));
        QVERIFY(!a.contains(QStringLiteral("--dry-run")));
        QCOMPARE(a.at(a.size() - 2), o.origen);
        QCOMPARE(a.last(), o.destino);
        QCOMPARE(a.count(QStringLiteral("--exclude")), exclusionesBase().size());
        const qsizetype e = a.indexOf(QStringLiteral("-e"));
        QCOMPARE(a.at(e + 1), QStringLiteral("ssh"));

        // La simulación no guarda versiones anteriores
        o.simular = true;
        o.backupDir.clear();
        const QStringList s = argumentosRespaldo(o);
        QVERIFY(s.contains(QStringLiteral("--dry-run")));
        QVERIFY(!s.contains(QStringLiteral("--backup")));
    }

    void argumentosDeRestauracion()
    {
        OpcionesRestaurar o;
        o.ssh = QStringLiteral("ssh");
        o.servidor = QStringLiteral("s@1.2.3.4");
        o.base = QStringLiteral("/R/ana/PC/");
        o.carpetaPersonal = QStringLiteral("/cygdrive/c/Users/Ana");
        QStringList a = argumentosRestaurar(o);
        QVERIFY(!a.contains(QStringLiteral("--delete")));
        QVERIFY(a.contains(QStringLiteral("--ignore-existing")));
        QCOMPARE(a.at(a.size() - 2), QStringLiteral("s@1.2.3.4:/R/ana/PC/"));
        QCOMPARE(a.last(), QStringLiteral("/cygdrive/c/Users/Ana/"));

        // Una carpeta suelta va a su carpeta madre (sin / al final: la carpeta entera)
        o.relativa = QStringLiteral("Documentos/Fotos");
        a = argumentosRestaurar(o);
        QCOMPARE(a.at(a.size() - 2), QStringLiteral("s@1.2.3.4:/R/ana/PC/Documentos/Fotos"));
        QCOMPARE(a.last(), QStringLiteral("/cygdrive/c/Users/Ana/Documentos/"));
        o.relativa = QStringLiteral("nota.txt");
        a = argumentosRestaurar(o);
        QCOMPARE(a.last(), QStringLiteral("/cygdrive/c/Users/Ana/"));

        // Reemplazando, lo que había queda guardado aparte
        o.reemplazar = true;
        o.reemplazados = QStringLiteral("/cygdrive/c/Users/Ana/AppData/Local/WM/reemplazados/x");
        a = argumentosRestaurar(o);
        QVERIFY(!a.contains(QStringLiteral("--ignore-existing")));
        QVERIFY(a.contains(QStringLiteral("--backup")));
        QVERIFY(a.contains(QStringLiteral("--backup-dir=/cygdrive/c/Users/Ana/AppData/Local/WM/reemplazados/x")));
        QVERIFY(!a.contains(QStringLiteral("--delete")));

        // Desde una versión anterior
        o.base = carpetaAnteriores(QStringLiteral("/R/ana/PC/")) + QStringLiteral("2026-11-26_09-05");
        o.relativa.clear();
        a = argumentosRestaurar(o);
        QCOMPARE(a.at(a.size() - 2), QStringLiteral("s@1.2.3.4:/R/ana/PC.anteriores/2026-11-26_09-05/"));
    }

    void argumentosDeLimpieza()
    {
        const QStringList a = argumentosLimpiar(QStringLiteral("ssh"), QStringLiteral("/tmp/vacia"), QStringLiteral("s@h:/R/ana/PC.anteriores/"),
                                                {QStringLiteral("2026-10-01_10-00"), QStringLiteral("*"), QStringLiteral("../x")});
        QVERIFY(a.contains(QStringLiteral("--delete")));
        QVERIFY(a.contains(QStringLiteral("--include=/2026-10-01_10-00/***")));
        QCOMPARE(a.filter(QStringLiteral("--include")).size(), 1);    // nada de comodines ni rutas raras
        QVERIFY(a.indexOf(QStringLiteral("--exclude=*")) > a.indexOf(QStringLiteral("--include=/2026-10-01_10-00/***")));
        QCOMPARE(a.at(a.size() - 2), QStringLiteral("/tmp/vacia/"));
        QCOMPARE(a.last(), QStringLiteral("s@h:/R/ana/PC.anteriores/"));
    }

    void ssh()
    {
        const QString c = comandoSsh(QStringLiteral("/x/ssh.exe"), QStringLiteral("/x/huellas"));
        QVERIFY(c.startsWith(QStringLiteral("\"/x/ssh.exe\" -F none -p 22 ")));
        QVERIFY(c.contains(QStringLiteral("StrictHostKeyChecking=yes")));
        QVERIFY(c.contains(QStringLiteral("PubkeyAuthentication=no")));    // solo contraseña
        QVERIFY(c.contains(QStringLiteral("UserKnownHostsFile=\"/x/huellas\"")));
        QVERIFY(comandoSsh(QStringLiteral("s"), QStringLiteral("h"), 2222).contains(QStringLiteral("-p 2222 ")));
    }

    void lector()
    {
        LectorRsync l;
        auto r = l.procesar(QStringLiteral("        2.00MB  99%  2.00GB/s    0:00:00 (xfr#1, to-chk=0/2)"));
        QCOMPARE(r.tipo, LectorRsync::Tipo::Progreso);
        QCOMPARE(r.porcentaje, 99);
        QCOMPARE(l.procesar(QStringLiteral("99% seguro.txt")).tipo, LectorRsync::Tipo::Archivo);
        QCOMPARE(l.procesar(QStringLiteral("backed up b.txt to ../PC.anteriores/2026-11-26_10-00/b.txt")).tipo, LectorRsync::Tipo::Respaldado);
        QCOMPARE(l.procesar(QStringLiteral("backed up sub/c.txt to ../PC.anteriores/2026-11-26_10-00/sub/c.txt")).tipo, LectorRsync::Tipo::Respaldado);
        QCOMPARE(l.procesar(QStringLiteral("(new) backup_dir is ../PC.anteriores/2026-11-26_10-00")).tipo, LectorRsync::Tipo::Nada);
        QCOMPARE(l.procesar(QStringLiteral("sending incremental file list")).tipo, LectorRsync::Tipo::Nada);
        QCOMPARE(l.procesar(QStringLiteral("IO error encountered -- skipping file deletion")).tipo, LectorRsync::Tipo::Aviso);
        QCOMPARE(l.respaldados(), 2);
        QVERIFY(!l.sinDatos());

        LectorRsync mala;
        mala.procesar(QStringLiteral("sebastian@1.2.3.4: Permission denied (publickey,password)."));
        mala.procesar(QStringLiteral("rsync: connection unexpectedly closed (0 bytes received so far) [sender]"));
        QVERIFY(mala.contrasenaMala());
        QVERIFY(!mala.huellaDistinta());
        QCOMPARE(clasificar(255, QProcess::NormalExit, false, mala), Final::ContrasenaMala);

        LectorRsync huella;
        huella.procesar(QStringLiteral("Host key verification failed."));
        QVERIFY(huella.huellaDistinta());
        QCOMPARE(clasificar(255, QProcess::NormalExit, false, huella), Final::HuellaDistinta);
    }

    void clasificacion()
    {
        const LectorRsync l;
        QCOMPARE(clasificar(0, QProcess::NormalExit, false, l), Final::Ok);
        QCOMPARE(clasificar(23, QProcess::NormalExit, false, l), Final::Incompleto);
        QCOMPARE(clasificar(24, QProcess::NormalExit, false, l), Final::Incompleto);
        QCOMPARE(clasificar(12, QProcess::NormalExit, false, l), Final::Error);
        // Se cerró de golpe sin que nadie cancele: falló, no «cancelado»
        QCOMPARE(clasificar(0, QProcess::CrashExit, false, l), Final::Fallo);
        QCOMPARE(clasificar(1, QProcess::CrashExit, true, l), Final::Cancelado);
    }
};

QTEST_GUILESS_MAIN(PruebaRespaldo)
#include "tst_respaldo.moc"
