// Windows-Manager: descargas de videos y música con yt-dlp, y respaldo de la carpeta personal
// con rsync. Los programas vienen en la carpeta «herramientas», al lado del .exe.

#include "VentanaPrincipal.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Windows-Manager"));
    QApplication::setStyle(QStringLiteral("Fusion"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/Logo.png")));

    // En Windows, Fusion rellena de azul la casilla desmarcada y parece que estuviera marcada
    app.setStyleSheet(QStringLiteral("QCheckBox::indicator:unchecked { background: palette(base); "
                                     "border: 1px solid palette(mid); width: 13px; height: 13px; }"));

    VentanaPrincipal ventana;
    ventana.show();
    return app.exec();
}
