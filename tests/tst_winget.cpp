// winget: lectura de las tablas de «search» y «upgrade» (en español y en inglés), limpieza de la salida
// (giros, barras de progreso, colores) y argumentos de cada pedido.

#include "Winget.h"

#include <QtTest>

class PruebaWinget : public QObject
{
    Q_OBJECT

private slots:
    void tablaBuscar()
    {
        const QString texto = QStringLiteral(
            "Nombre               Id                      Versión      Coincidencia   Origen\n"
            "---------------------------------------------------------------------------------\n"
            "VLC media player     VideoLAN.VLC            3.0.21                      winget\n"
            "VLC UWP              9NBLGGH4VVNH            Unknown                     msstore\n"
            "Otro VLC             Alguien.VLC.Nightly     4.0.0-dev    Tag: vlc       winget\n");
        const QList<winget::Paquete> p = winget::leerTabla(texto);
        QCOMPARE(p.size(), 3);
        QCOMPARE(p.at(0).nombre, QStringLiteral("VLC media player"));
        QCOMPARE(p.at(0).id, QStringLiteral("VideoLAN.VLC"));
        QCOMPARE(p.at(0).version, QStringLiteral("3.0.21"));
        QCOMPARE(p.at(0).origen, QStringLiteral("winget"));
        QVERIFY(p.at(0).disponible.isEmpty());
        QCOMPARE(p.at(1).origen, QStringLiteral("msstore"));
        QCOMPARE(p.at(2).id, QStringLiteral("Alguien.VLC.Nightly"));
    }

    void tablaActualizar()
    {
        // Con giros y una barra antes de la tabla, como la deja winget al leerla de un programa
        const QByteArray salida =
            "   - \r   \\ \r   | \r"
            "  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x92\xe2\x96\x92  1.00 MB / 2.00 MB\r\n"
            "Nombre                    Id                    Versi\xc3\xb3n        Disponible   Origen\r\n"
            "-------------------------------------------------------------------------------------\r\n"
            "Mozilla Firefox (x64 es)  Mozilla.Firefox       130.0          131.0        winget\r\n"
            "7-Zip 24.07 (x64)         7zip.7zip             24.07          24.08        winget\r\n"
            "2 actualizaciones disponibles.\r\n";
        const QString texto = winget::limpiar(salida);
        QVERIFY(!texto.contains(u'\r'));
        const QList<winget::Paquete> p = winget::leerTabla(texto);
        QCOMPARE(p.size(), 2);
        QCOMPARE(p.at(0).nombre, QStringLiteral("Mozilla Firefox (x64 es)"));
        QCOMPARE(p.at(0).id, QStringLiteral("Mozilla.Firefox"));
        QCOMPARE(p.at(0).version, QStringLiteral("130.0"));
        QCOMPARE(p.at(0).disponible, QStringLiteral("131.0"));
        QCOMPARE(p.at(1).id, QStringLiteral("7zip.7zip"));
        QCOMPARE(p.at(1).disponible, QStringLiteral("24.08"));
    }

    void tablaEnIngles()
    {
        const QString texto = QStringLiteral(
            "Name             Id               Version  Available Source\n"
            "-----------------------------------------------------------\n"
            "Git              Git.Git          2.45.0   2.46.0    winget\n"
            "1 upgrades available.\n"
            "\n"
            "The following packages have an upgrade available, but require explicit targeting for upgrade:\n"
            "Name   Id        Version  Available\n"
            "-----------------------------------\n"
            "Discord Discord.Discord 1.0 1.1\n");
        const QList<winget::Paquete> p = winget::leerTabla(texto);
        QCOMPARE(p.size(), 1);    // solo la primera tabla
        QCOMPARE(p.at(0).id, QStringLiteral("Git.Git"));
        QCOMPARE(p.at(0).disponible, QStringLiteral("2.46.0"));
        QCOMPARE(p.at(0).origen, QStringLiteral("winget"));
    }

    void sinTabla()
    {
        QVERIFY(winget::leerTabla(QStringLiteral("No se encontró ningún paquete que coincida con los criterios de entrada.")).isEmpty());
        QVERIFY(winget::leerTabla(QString()).isEmpty());
    }

    void progreso()
    {
        QVERIFY(winget::esProgreso(QStringLiteral("  ██████▒▒▒▒  10.0 MB / 20.0 MB")));
        QVERIFY(winget::esProgreso(QStringLiteral(" 45% ")));
        QVERIFY(!winget::esProgreso(QStringLiteral("Instalado correctamente")));
        QCOMPARE(winget::limpiar("\x1b[32mListo\x1b[0m\n"), QStringLiteral("Listo\n"));
    }

    void argumentos()
    {
        QCOMPARE(winget::argumentosBuscar(QStringLiteral("vlc media")),
                 (QStringList{QStringLiteral("search"), QStringLiteral("--query"), QStringLiteral("vlc media"),
                              QStringLiteral("--accept-source-agreements")}));
        const QStringList instalar = winget::argumentosInstalar(QStringLiteral("VideoLAN.VLC"));
        QCOMPARE(instalar.mid(0, 4), (QStringList{QStringLiteral("install"), QStringLiteral("--id"), QStringLiteral("VideoLAN.VLC"),
                                                  QStringLiteral("--exact")}));
        QVERIFY(instalar.contains(QStringLiteral("--accept-package-agreements")));
        QVERIFY(winget::argumentosActualizar({}).contains(QStringLiteral("--all")));
        QVERIFY(!winget::argumentosActualizar(QStringLiteral("Git.Git")).contains(QStringLiteral("--all")));
        QCOMPARE(winget::argumentosActualizables().first(), QStringLiteral("upgrade"));
    }

    void codigos()
    {
        QVERIFY(winget::mensajeCodigo(0).isEmpty());
        QVERIFY(winget::mensajeCodigo(int(0x8A150014u)).contains(QStringLiteral("No se encontró")));
        QVERIFY(winget::mensajeCodigo(int(0x8A15002Bu)).contains(QStringLiteral("más nueva")));
        QVERIFY(winget::mensajeCodigo(int(0x8A150001u)).contains(QStringLiteral("0x8a150001")));
    }
};

QTEST_GUILESS_MAIN(PruebaWinget)
#include "tst_winget.moc"
