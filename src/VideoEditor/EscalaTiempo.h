// EscalaTiempo.h
// Capa: VIEW (pero SIN MFC, para poder probarla con tests unitarios).
//
// Conversión entre tiempo del timeline (TimeUs) y coordenada X en píxeles de la ventana
// del timeline, con zoom (píxeles por segundo) y scroll horizontal (desplazamiento).
//
//   x = origenX + t * pxPorSegundo - scrollPx
//
// También calcula el pegado (imán) de los clips: el umbral se define en píxeles, así que
// en tiempo depende del zoom (con más zoom, hay que acercarse más en segundos).
#pragma once

#include "EditorModel/Ids.h"
#include "EditorModel/Tiempo.h"

#include <optional>
#include <vector>

class EscalaTiempo {
public:
    static constexpr double kZoomMinimo = 2.0;     // px por segundo
    static constexpr double kZoomMaximo = 2000.0;
    static constexpr int kImanPx = 8;  // distancia en pantalla a la que un clip se pega

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
    // Distancia en tiempo a la que un clip se pega con el zoom actual.
    model::TimeUs umbralIman() const { return pxADuracion(kImanPx); }

    // Zoom manteniendo fijo el instante que está bajo la coordenada x (el mouse).
    void zoomAlrededorDe(int x, double factor);

    // Ancho total en píxeles necesario para mostrar `duracion` (para la barra de scroll).
    int anchoContenido(model::TimeUs duracion) const;

private:
    int origenX_ = 0;
    double pxPorSegundo_ = 50.0;
    int scrollPx_ = 0;
};

// Tramo [inicio, fin) de un clip en una pista, para calcular el pegado.
struct TramoIman {
    model::Id pista = model::kSinId;
    model::TimeUs inicio = 0;
    model::TimeUs fin = 0;
};

struct PegadoIman {
    model::TimeUs delta = 0;  // desplazamiento ajustado
    model::TimeUs punto = 0;  // instante del timeline donde quedan pegados
};

// Busca el pegado más cercano para `moviles` desplazados `delta`. Solo cuenta si la
// diferencia con `delta` no supera `umbral` y ningún móvil queda antes de 0.
//  1) Primero en la misma pista: el inicio de un móvil contra el fin de un fijo, su fin
//     contra el inicio de un fijo, o su inicio contra el 0 del timeline.
//  2) Si no hay ninguno, alineación con otras pistas: cualquier borde contra cualquier
//     borde. Como la pista propia va primero, esto nunca hace cruzar a un vecino propio:
//     para cruzarlo habría que pasar por su borde, y ese ya estaría dentro del umbral.
std::optional<PegadoIman> buscarPegado(const std::vector<TramoIman>& moviles, const std::vector<TramoIman>& fijos,
                                       model::TimeUs delta, model::TimeUs umbral);
