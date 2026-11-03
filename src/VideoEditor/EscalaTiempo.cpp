// EscalaTiempo.cpp
// Capa: VIEW (sin MFC). Ver EscalaTiempo.h.
#include "EscalaTiempo.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

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

std::optional<PegadoIman> buscarPegado(const std::vector<TramoIman>& moviles, const std::vector<TramoIman>& fijos,
                                       model::TimeUs delta, model::TimeUs umbral) {
    if (moviles.empty()) return std::nullopt;
    model::TimeUs inicioMinimo = moviles.front().inicio;
    for (const TramoIman& m : moviles) inicioMinimo = std::min(inicioMinimo, m.inicio);

    std::optional<PegadoIman> mejor;
    model::TimeUs mejorDistancia = umbral;
    auto probar = [&](model::TimeUs candidato, model::TimeUs punto) {
        if (inicioMinimo + candidato < 0) return;
        const model::TimeUs distancia = std::llabs(candidato - delta);
        if (distancia > mejorDistancia || (mejor && distancia == mejorDistancia)) return;
        mejorDistancia = distancia;
        mejor = PegadoIman{candidato, punto};
    };
    for (const TramoIman& m : moviles) {
        probar(-m.inicio, 0);
        for (const TramoIman& f : fijos) {
            if (f.pista != m.pista) continue;
            probar(f.fin - m.inicio, f.fin);
            probar(f.inicio - m.fin, f.inicio);
        }
    }
    if (mejor) return mejor;

    for (const TramoIman& m : moviles) {
        for (const TramoIman& f : fijos) {
            if (f.pista == m.pista) continue;
            probar(f.inicio - m.inicio, f.inicio);
            probar(f.fin - m.inicio, f.fin);
            probar(f.inicio - m.fin, f.inicio);
            probar(f.fin - m.fin, f.fin);
        }
    }
    return mejor;
}
