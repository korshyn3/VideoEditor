// EscalaTiempo.h
// Capa: VIEW (pero SIN MFC, para poder probarla con tests unitarios).
//
// Conversión entre tiempo del timeline (TimeUs) y coordenada X en píxeles de la ventana
// del timeline, con zoom (píxeles por segundo) y scroll horizontal (desplazamiento).
//
//   x = origenX + t * pxPorSegundo - scrollPx
#pragma once

#include "EditorModel/Tiempo.h"

class EscalaTiempo {
public:
    static constexpr double kZoomMinimo = 2.0;     // px por segundo
    static constexpr double kZoomMaximo = 2000.0;

    // origenX: dónde empieza el área de clips (a la derecha de los nombres de pista).
    void setOrigenX(int origenX) { origenX_ = origenX; }
    int origenX() const { return origenX_; }

    double pxPorSegundo() const { return pxPorSegundo_; }
    void setPxPorSegundo(double pps);

    int scrollPx() const { return scrollPx_; }
    void setScrollPx(int px) { scrollPx_ = px < 0 ? 0 : px; }

    int tiempoAX(model::TimeUs t) const;
    model::TimeUs xATiempo(int x) const;
    // Duración equivalente a un ancho en píxeles (para arrastres).
    model::TimeUs pxADuracion(int px) const;

    // Zoom manteniendo fijo el instante que está bajo la coordenada x (el mouse).
    void zoomAlrededorDe(int x, double factor);

    // Ancho total en píxeles necesario para mostrar `duracion` (para la barra de scroll).
    int anchoContenido(model::TimeUs duracion) const;

private:
    int origenX_ = 0;
    double pxPorSegundo_ = 50.0;
    int scrollPx_ = 0;
};
