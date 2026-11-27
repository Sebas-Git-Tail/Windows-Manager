// winget sin ventana (se prueba en Linux, tests/): los argumentos de cada pedido y la lectura de
// sus tablas («winget search» y «winget upgrade») y de sus códigos de salida.

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace winget {

struct Paquete {
    QString nombre;
    QString id;
    QString version;
    QString disponible;     // solo en «upgrade»
    QString origen;
};

// La salida de winget como texto: sin los giros ni las barras de progreso (que reescribe con \r) ni
// secuencias de color
QString limpiar(const QByteArray &salida);
// Una línea es de progreso (barra con bloques o porcentaje suelto): se muestra aparte, no en el registro
bool esProgreso(const QString &linea);

// La primera tabla de la salida (encabezado, renglón de guiones y filas). Las columnas se ubican por
// dónde empieza cada palabra del encabezado; sirve en español y en inglés.
QList<Paquete> leerTabla(const QString &texto);

QStringList argumentosBuscar(const QString &texto);
QStringList argumentosInstalar(const QString &id);
QStringList argumentosActualizables();
QStringList argumentosActualizar(const QString &id);    // id vacío: todos

// Qué quiere decir el código de salida (HRESULT de winget); vacío si es 0
QString mensajeCodigo(int codigo);

}
