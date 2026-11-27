// Lógica de las descargas sin ventana (se prueba en Linux, tests/): nombres de archivo seguros para
// Windows y nombres libres para no pisar nada (Descargas en masa y «Bajar con el servidor»).

#pragma once

#include <QDir>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace descargas {

// Las URL escritas, una por línea (sin renglones vacíos)
QStringList urlsDeTexto(const QString &texto);

// Un nombre que Windows acepta: sin \ / : * ? " < > | ni caracteres de control, sin punto ni espacio
// al final y sin los nombres reservados (CON, NUL, COM1…). Nunca vacío.
QString nombreSeguro(const QString &nombre);

// El nombre del archivo de una URL: lo último del camino, decodificado (sin «?…»). Si no tiene,
// «descarga-<numero>».
QString nombreDesdeUrl(const QUrl &url, int numero);

// El mismo nombre si no existe en la carpeta; si no «base (2).ext», «base (3).ext»… (las extensiones
// dobles como .tar.gz quedan juntas). Igual que Descargas-en-masa.sh de Arch-Linux-Manager.
QString nombreLibre(const QDir &carpeta, const QString &nombre);

}
