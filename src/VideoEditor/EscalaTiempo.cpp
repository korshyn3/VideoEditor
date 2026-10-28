// EscalaTiempo.cpp
// Capa: VIEW (sin MFC). Ver EscalaTiempo.h.
#include "EscalaTiempo.h"

#include <algorithm>
#include <cmath>

void EscalaTiempo::setPxPorSegundo(double pps) {
    pxPorSegundo_ = std::clamp(pps, kZoomMinimo, kZoomMaximo);
}

int EscalaTiempo::tiempoAX(model::TimeUs t) const {
    const double px = model::tiempoASegundos(t) * pxPorSegundo_;
    return origenX_ + static_cast<int>(std::lround(px)) - scrollPx_;
}

model::TimeUs EscalaTiempo::xATiempo(int x) const {
    return pxADuracion(x - origenX_ + scrollPx_);
}

model::TimeUs EscalaTiempo::pxADuracion(int px) const {
    return model::segundosATiempo(static_cast<double>(px) / pxPorSegundo_);
}

void EscalaTiempo::zoomAlrededorDe(int x, double factor) {
    const model::TimeUs bajoMouse = xATiempo(x);
    setPxPorSegundo(pxPorSegundo_ * factor);
    // Se recalcula el scroll para que `bajoMouse` siga en la misma x.
    const double pxDesdeCero = model::tiempoASegundos(bajoMouse) * pxPorSegundo_;
    setScrollPx(static_cast<int>(std::lround(pxDesdeCero)) - (x - origenX_));
}

int EscalaTiempo::anchoContenido(model::TimeUs duracion) const {
    return static_cast<int>(std::lround(model::tiempoASegundos(duracion) * pxPorSegundo_));
}
