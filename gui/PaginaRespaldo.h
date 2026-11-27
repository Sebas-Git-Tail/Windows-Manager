// Página «Respaldo»: copia la carpeta personal al servidor con rsync, como Respaldo-Linux en el
// Manager de Linux. Como allá, lo que se borró de la computadora también se borra del respaldo
// (--delete), pero lo que se borra o se pisa en el servidor queda 30 días en «versiones anteriores»
// (ver LogicaRespaldo.h). Cada computadora respalda en la carpeta de su dueño:
// Respaldos/<persona>/<computadora>/. Los dos nombres quedan guardados.
// También restaura (todo, una carpeta o un archivo, del último respaldo o de una versión anterior) y
// deja elegir carpetas que no se copian.

#pragma once

#include "LogicaRespaldo.h"
#include "ResumenSimulacion.h"

#include <QProcess>
#include <QWidget>

#include <memory>

namespace Ui { class PaginaRespaldo; }
class QTcpSocket;
class QTemporaryDir;

class PaginaRespaldo : public QWidget
{
    Q_OBJECT

public:
    explicit PaginaRespaldo(QWidget *parent = nullptr);
    ~PaginaRespaldo() override;

    bool ocupada() const;
    void cancelarYEsperar();

signals:
    void ocupacionCambiada();

protected:
    void showEvent(QShowEvent *evento) override;

private slots:
    void empezar();
    void restaurar();
    void buscarVersiones();
    void cancelar();
    void leerSalida();
    void terminoProceso(int codigo, QProcess::ExitStatus estado);

private:
    // Lo que se está haciendo. Un respaldo real sigue con Listar y Limpiar (versiones de más de 30 días)
    enum class Paso { Ninguno, Respaldo, Listar, Limpiar, Restaurar, Versiones };

    bool validar();
    void iniciar(Paso primero);
    void probarServidor(int indice);
    void correr(Paso paso);
    void terminar();
    void terminoRespaldo(respaldo::Final final, int codigo);
    void terminoRestaurar(respaldo::Final final, int codigo);
    void terminoVersiones(respaldo::Final final, int codigo);
    bool mostrarError(respaldo::Final final, int codigo, const QString &que);
    void procesarLinea(const QString &linea);
    void anotar(const QString &texto);
    void mostrarResumen(bool restauracion);
    void habilitarFormulario(bool habilitado);
    void mostrarDescripcion();
    void mostrarUltimo();
    void cargarExclusiones();
    void guardarExclusiones();
    void agregarExclusion();
    void elegirParaRestaurar(bool carpeta);
    QString persona() const;       // normalizada; vacía si lo escrito no sirve como carpeta
    QString computadora() const;   // vacío si lo escrito no sirve como carpeta
    QString rutaEnServidor() const;
    QString comandoSsh() const;
    QStringList extras() const;    // carpetas que el usuario no quiere copiar
    void guardarNombres();
    bool confirmarDestino();
    bool confirmarRestauracion();

    Ui::PaginaRespaldo *ui;
    QProcess proceso;
    QTcpSocket *prueba = nullptr;
    QString contrasena;            // se guarda solo mientras dura la tarea (puede usar varias conexiones)
    QString servidor;              // la dirección que contestó
    QByteArray pendiente;
    ResumenSimulacion resumen;
    respaldo::LectorRsync lector;
    Paso paso = Paso::Ninguno;
    Paso primerPaso = Paso::Ninguno;
    bool simular = false;
    QString fechaRespaldo;          // AAAA-MM-DD_HH-MM de este respaldo (carpeta de versiones anteriores)
    int respaldados = 0;            // archivos que fueron a «anteriores» en este respaldo
    QStringList viejas;             // versiones anteriores de más de 30 días (se borran)
    std::unique_ptr<QTemporaryDir> vacia;   // carpeta vacía para borrar las viejas con rsync
    QString relativaRestaurar;      // lo que se restaura (vacío: todo)
    QString baseRestaurar;          // carpeta del servidor de donde se restaura
    QString reemplazados;           // carpeta local donde queda lo que se reemplazó al restaurar
    bool cancelado = false;
    bool trabajando = false;
};
