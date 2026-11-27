// Página «Programas»: buscar, instalar y actualizar programas con winget (el instalador de programas
// de Windows, que viene con «Instalador de aplicación» de Microsoft Store). Muestra lo que va diciendo
// winget. Si no está, explica cómo conseguirlo. La lectura de las tablas está en Winget.h.

#pragma once

#include <QProcess>
#include <QWidget>

namespace Ui { class PaginaProgramas; }

class PaginaProgramas : public QWidget
{
    Q_OBJECT

public:
    explicit PaginaProgramas(QWidget *parent = nullptr);
    ~PaginaProgramas() override;

    bool ocupada() const;
    void cancelarYEsperar();

signals:
    void ocupacionCambiada();

protected:
    void showEvent(QShowEvent *evento) override;

private:
    // Lo que está haciendo winget (Buscar y Actualizables llenan la lista; el resto la cambia)
    enum class Tarea { Ninguna, Buscar, Actualizables, Instalar, Actualizar };

    bool revisarWinget();
    void correr(Tarea tarea, const QStringList &argumentos, const QString &estado);
    void leerSalida();
    void procesarLinea(const QString &linea);
    void termino(int codigo, QProcess::ExitStatus estado);
    void actualizarBotones();
    void habilitar(bool habilitado);
    QString idElegido() const;

    Ui::PaginaProgramas *ui;
    QProcess proceso;
    QString rutaWinget;
    Tarea tarea = Tarea::Ninguna;
    Tarea mostrando = Tarea::Ninguna;     // qué hay en la lista (resultados de búsqueda o actualizables)
    QByteArray salida;                    // toda la salida (para leer la tabla al final)
    QByteArray pendiente;                 // lo que llegó sin fin de línea todavía
    bool cancelado = false;
    bool revisado = false;
};
