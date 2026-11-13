// Página «Respaldo»: copia la carpeta personal al servidor con rsync, como Respaldo-Linux en el
// Manager de Linux. Como allá, lo que se borró de la computadora también se borra del respaldo
// (--delete). Cada computadora respalda en la carpeta de su dueño:
// Respaldos/<persona>/<computadora>/. Los dos nombres quedan guardados.

#pragma once

#include "ResumenSimulacion.h"

#include <QProcess>
#include <QWidget>

namespace Ui { class PaginaRespaldo; }
class QTcpSocket;

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
    void cancelar();
    void leerSalida();
    void terminoProceso(int codigo, QProcess::ExitStatus estado);

private:
    void probarServidor(int indice);
    void copiar(const QString &servidor);
    void procesarLinea(const QString &linea);
    void anotar(const QString &texto);
    void mostrarResumen();
    void habilitarFormulario(bool habilitado);
    void mostrarDescripcion();
    QString persona() const;       // normalizada; vacía si lo escrito no sirve como carpeta
    QString computadora() const;   // vacío si lo escrito no sirve como carpeta
    QString rutaEnServidor() const;
    void guardarNombres();
    bool confirmarDestino();

    Ui::PaginaRespaldo *ui;
    QProcess proceso;
    QTcpSocket *prueba = nullptr;
    QString contrasena;            // se guarda solo mientras se busca el servidor
    QByteArray pendiente;
    ResumenSimulacion resumen;
    QString errorConexion;
    bool sinDatos = false;         // rsync no llegó a recibir nada del servidor
    bool cancelado = false;
    bool trabajando = false;
};
