// Los programas que vienen con la app (yt-dlp, ffmpeg, QuickJS, rsync, ssh) y lo necesario
// para ejecutarlos. Están en la carpeta «herramientas», al lado del .exe.

#pragma once

#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace Herramientas {

QString programa(const QString &nombre);    // ruta de herramientas/<nombre>
QStringList faltantes();                    // los programas que no están en la carpeta
QProcessEnvironment ambiente();
QString datos();                            // %LOCALAPPDATA%/Windows-Manager
QString temporal();

// rsync y ssh son de MSYS2: las rutas van como /cygdrive/c/Users/... («C:» sería un host remoto)
QString rutaMsys(const QString &ruta);

// yt-dlp
QStringList argumentosYtDlp();              // ffmpeg, QuickJS (desafíos de YouTube) y caché
bool ytDlpDesactualizado();                 // se actualiza una vez por día
void marcarYtDlpActualizado();

// Corta un proceso con todos sus hijos (yt-dlp lanza ffmpeg; rsync lanza ssh) y espera
void cortar(QProcess &proceso);

}
