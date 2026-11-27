// Descargas: URLs del cuadro de texto, nombres seguros para Windows y nombres libres (no pisar nada).

#include "LogicaDescargas.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace descargas;

class PruebaDescargas : public QObject
{
    Q_OBJECT

private slots:
    void urls()
    {
        QCOMPARE(urlsDeTexto(QStringLiteral(" https://a/x.zip \n\n  \nhttps://b/y.pdf\r\n")),
                 (QStringList{QStringLiteral("https://a/x.zip"), QStringLiteral("https://b/y.pdf")}));
        QVERIFY(urlsDeTexto(QStringLiteral("  \n ")).isEmpty());
    }

    void seguro()
    {
        QCOMPARE(nombreSeguro(QStringLiteral("a:b*c?.txt")), QStringLiteral("a_b_c_.txt"));
        QCOMPARE(nombreSeguro(QStringLiteral("final. . ")), QStringLiteral("final"));
        QCOMPARE(nombreSeguro(QStringLiteral("CON.txt")), QStringLiteral("_CON.txt"));
        QCOMPARE(nombreSeguro(QStringLiteral("nul")), QStringLiteral("_nul"));
        QCOMPARE(nombreSeguro(QStringLiteral("console.txt")), QStringLiteral("console.txt"));
        QCOMPARE(nombreSeguro(QStringLiteral("...")), QStringLiteral("_"));
        QCOMPARE(nombreSeguro(QStringLiteral("Canción «1».mkv")), QStringLiteral("Canción «1».mkv"));
    }

    void desdeUrl()
    {
        QCOMPARE(nombreDesdeUrl(QUrl(QStringLiteral("https://x.com/dir/Mi%20archivo.pdf?token=1")), 1), QStringLiteral("Mi archivo.pdf"));
        QCOMPARE(nombreDesdeUrl(QUrl(QStringLiteral("https://x.com/")), 3), QStringLiteral("descarga-3"));
        QCOMPARE(nombreDesdeUrl(QUrl(QStringLiteral("https://x.com/a/..")), 2), QStringLiteral("descarga-2"));
        QCOMPARE(nombreDesdeUrl(QUrl(QStringLiteral("https://x.com/a%2Fb%3A.txt")), 1), QStringLiteral("a_b_.txt"));
    }

    void libre()
    {
        QTemporaryDir carpeta;
        QVERIFY(carpeta.isValid());
        const QDir dir(carpeta.path());
        QCOMPARE(nombreLibre(dir, QStringLiteral("x.zip")), QStringLiteral("x.zip"));
        for (const QString &n : {QStringLiteral("x.zip"), QStringLiteral("x (2).zip"), QStringLiteral("datos.tar.gz"), QStringLiteral("LEEME")}) {
            QFile f(dir.filePath(n));
            QVERIFY(f.open(QIODevice::WriteOnly));
        }
        QCOMPARE(nombreLibre(dir, QStringLiteral("x.zip")), QStringLiteral("x (3).zip"));
        QCOMPARE(nombreLibre(dir, QStringLiteral("datos.tar.gz")), QStringLiteral("datos (2).tar.gz"));
        QCOMPARE(nombreLibre(dir, QStringLiteral("LEEME")), QStringLiteral("LEEME (2)"));
    }
};

QTEST_GUILESS_MAIN(PruebaDescargas)
#include "tst_descargas_wm.moc"
