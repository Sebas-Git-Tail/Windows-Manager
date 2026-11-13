// Ventana principal: las secciones a la izquierda y su página a la derecha, como en el Manager.

#pragma once

#include <QWidget>

namespace Ui { class VentanaPrincipal; }
class PaginaDescargas;
class PaginaRespaldo;

class VentanaPrincipal : public QWidget
{
    Q_OBJECT

public:
    explicit VentanaPrincipal(QWidget *parent = nullptr);
    ~VentanaPrincipal() override;

protected:
    void closeEvent(QCloseEvent *evento) override;

private:
    void actualizarEnergia();
    bool hayTarea() const;

    Ui::VentanaPrincipal *ui;
    PaginaDescargas *videos;
    PaginaDescargas *musica;
    PaginaRespaldo *respaldo;
};
