// Páginas «Videos», «Música» y «Descargas en masa»: la misma página con tres modos.
// Videos y Música hacen lo mismo que Logica/Descargar.sh en el Manager de Linux: las mismas opciones
// de yt-dlp y una URL por vez, leyendo el avance de su salida. En Videos, si se pide, antes consulta qué
// calidades, códecs, pistas de audio y subtítulos ofrece cada URL y deja elegir (DialogoPistas).
// Con «Bajar con el servidor», el servidor de casa baja con sus cookies y esta computadora trae los
// archivos terminados (PaginaDescargasServidor.cpp, la misma API que usa Android-Manager).
// Descargas en masa (como Descargas-en-masa.sh de Linux): una lista de URL de archivos, sin pisar
// ninguno que ya esté. Lo descargado queda en la lista aunque se cierre la app.

#pragma once

#include "ConsultaVideo.h"

#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QWidget>

namespace Ui { class PaginaDescargas; }
class QNetworkAccessManager;
class QNetworkReply;
class QTemporaryFile;

class PaginaDescargas : public QWidget
{
    Q_OBJECT

public:
    enum class Tipo { Videos, Musica, Archivos };

    PaginaDescargas(Tipo tipo, QWidget *parent = nullptr);
    ~PaginaDescargas() override;

    bool ocupada() const;
    void cancelarYEsperar();

signals:
    void ocupacionCambiada();

private slots:
    void descargar();
    void cancelar();
    void leerSalida();
    void terminoProceso(int codigo, QProcess::ExitStatus estado);
    void elegirCarpeta();
    void elegirCookies();
    void ver();
    void abrirCarpeta();
    void moverAPapelera();

private:
    void empezar();
    void siguienteConsulta();
    void terminoConsulta(int codigo, QProcess::ExitStatus estado);
    void siguienteUrl();
    void procesarLinea(const QString &linea);
    void registrarTerminados(const QString &miniatura);
    void finalizar();
    void anotar(const QString &texto);
    void ponerEstado(const QString &texto);
    void ponerProgreso(int archivo, int hechas);    // archivo -1: sin porcentaje (barra que va y viene)
    void habilitarFormulario(bool habilitado);
    void actualizarOpciones();
    QString destino() const;
    QString carpetaMiniaturas() const;
    QString archivoRutas() const;
    QString ultimaMiniatura() const;
    QString navegadorCookies() const;
    QString archivoCookies() const;
    QStringList argumentosCookies() const;
    QString carpetaSubtitulos() const;
    bool musica() const { return tipo == Tipo::Musica; }

    // Lo descargado (persiste entre aperturas)
    QString archivoLista() const;
    QString carpetaMiniaturasGuardadas() const;
    QString nuevaMiniatura() const;                 // ruta libre en carpetaMiniaturasGuardadas()
    void cargarDescargados();
    void guardarDescargados();
    void agregarDescargado(const QString &nombre, const QString &ruta, const QString &miniatura);

    // Descargas en masa
    void siguienteArchivo();
    void terminoArchivo();

    // Con el servidor (PaginaDescargasServidor.cpp)
    struct PorBajar {
        QString carpeta;
        QString nombre;
        qint64 tamano = 0;
        bool miniatura = false;
    };
    QUrl urlServidor(const QString &ruta) const;
    static QString mensajeDe(int codigo, const QByteArray &cuerpo);
    void remotoEmpezar();
    void remotoSiguienteConsulta();
    void remotoCrearTrabajo();
    void remotoPreguntar();
    void remotoProcesar(const QJsonObject &trabajo);
    void remotoSiguienteArchivo();
    void remotoMostrarTransferencia();
    void remotoTerminoArchivo(int codigo, const PorBajar &archivo, const QString &final, const QString &miniatura);
    void remotoQuizasTerminar();
    void remotoFallo(const QString &mensaje);
    void remotoCancelar();

    Ui::PaginaDescargas *ui;
    Tipo tipo;
    QProcess proceso;
    QStringList cola;
    int total = 0;
    int indice = 0;
    int listas = 0;
    int fallidas = 0;
    bool actualizando = false;
    bool consultando = false;       // paso previo: «yt-dlp -J» de cada URL
    QStringList urlsPedidas;        // todas las URL de esta descarga (la consulta no las consume)
    OfertaVideo oferta;             // lo que ofrecen (suma de las consultas)
    Eleccion eleccion;              // lo elegido en el diálogo (vacío: lo de siempre, en MKV)
    bool cancelado = false;
    QString archivoActual;
    QString mensajeError;
    QString ultimoPorcentaje;

    // Descargas en masa
    QNetworkAccessManager *red = nullptr;
    QNetworkReply *respuesta = nullptr;
    QTemporaryFile *parcial = nullptr;

    // Con el servidor
    bool remoto = false;            // esta descarga va por el servidor
    int vueltaRemota = 0;           // lo que llegue de una descarga anterior se ignora
    QString trabajoRemoto;          // id del trabajo en el servidor (vacío: todavía no se creó)
    QString estadoRemoto;
    QJsonObject ultimoRemoto;       // la última respuesta del servidor
    QList<PorBajar> porBajar;       // terminados en el servidor que faltan traer
    QSet<QString> vistos;
    PorBajar enCamino;
    QString rutaEnCamino;
    bool trayendo = false;
    int hechasRemotas = 0;
    int fallosSeguidos = 0;
    int traidosFallidos = 0;
    QTimer temporizadorRemoto;      // cada cuánto se pregunta cómo va
    QTimer temporizadorTransferencia;
};
