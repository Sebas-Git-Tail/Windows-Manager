// Windows-Manager: descargas de videos y música con yt-dlp, y respaldo de la carpeta personal
// con rsync. Los programas vienen en la carpeta «herramientas», al lado del .exe.

#include "VentanaPrincipal.h"
#include "Piezas.h"
#include "Tema.h"
#include "Traducciones.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    apps::cargarTraduccionesQt();   // botones estándar en español (comun/apps; Android y Windows no traen la traducción)
    QApplication::setApplicationName(QStringLiteral("Windows-Manager"));
    QApplication::setApplicationVersion(QStringLiteral(VERSION_PROGRAMA));
    tema::aplicar(app);             // tema oscuro común con el celeste del sitio (comun/ui)
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/Logo.png")));

    VentanaPrincipal ventana;
    tema::recordarVentana(&ventana);   // mismo tamaño (y maximizada o en pantalla completa) que la última vez
    ventana.show();
    return app.exec();
}
