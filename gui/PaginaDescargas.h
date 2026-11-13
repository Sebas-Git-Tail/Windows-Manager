// Páginas «Descargar Vídeos» y «Descargar Música»: la misma página con dos modos. Hace lo
// mismo que Logica/Descargar.sh en el Manager de Linux: las mismas opciones de yt-dlp y una
// URL por vez, leyendo el avance de su salida. En Vídeos, si se pide, antes consulta qué
// calidades, códecs, pistas de audio y subtítulos ofrece cada URL y deja elegir (DialogoPistas).

#pragma once

#include "ConsultaVideo.h"

#include <QProcess>
#include <QStringList>
#include <QWidget>

namespace Ui { class PaginaDescargas; }

class PaginaDescargas : public QWidget
{
    Q_OBJECT

public:
    enum class Tipo { Videos, Musica };

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
    void habilitarFormulario(bool habilitado);
    QString destino() const;
    QString carpetaMiniaturas() const;
    QString archivoRutas() const;
    QString ultimaMiniatura() const;
    QString navegadorCookies() const;
    QString carpetaSubtitulos() const;

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
};
