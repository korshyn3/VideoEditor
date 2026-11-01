// PlaybackEngine.h
// Capa: ENGINE (API pública, sin tipos de FFmpeg ni de XAudio2).
//
// Reproduce el timeline para el preview en un HILO PROPIO, para no congelar la interfaz:
//   - recibe órdenes (timeline nuevo, seek, play, pausa) con la política "la última gana";
//   - construye el grafo (FilterGraphBuilder + GraphRunner) a la resolución del preview;
//   - entrega cuadros BGRA (lo que entiende StretchDIBits) en una cola corta;
//   - reproduce el audio con XAudio2, que además hace de reloj maestro.
// La interfaz pide con tomarCuadro() el cuadro que corresponde al reloj; los cuadros que
// llegan tarde se descartan (si el equipo no da abasto, el video salta pero sigue en
// sincronía con el audio).
#pragma once

#include "EditorEngine/FilterGraphBuilder.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace engine {

// Un cuadro listo para pintar: píxeles BGRA de arriba a abajo (width*4 bytes por fila).
// width == 0 significa "nada que mostrar" (pantalla negra).
struct FrameBGRA {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
    model::TimeUs pts = 0;
};

class PlaybackEngine {
public:
    static constexpr int kAnchoPreview = 960;

    // `aviso` se llama DESDE EL HILO DE RENDER cuando hay algo nuevo para la interfaz
    // (cuadro en pausa, fin de la reproducción, error). Debe ser rápido: típicamente un
    // PostMessage a la ventana.
    explicit PlaybackEngine(std::function<void()> aviso = {});
    ~PlaybackEngine();
    PlaybackEngine(const PlaybackEngine&) = delete;
    PlaybackEngine& operator=(const PlaybackEngine&) = delete;

    void setTimeline(TimelineSnapshot snap);  // reconstruye en la posición actual
    void seek(model::TimeUs t);
    void play();
    void pause();

    bool reproduciendo() const;
    model::TimeUs reloj() const;  // posición actual de la reproducción
    // Último cuadro que ya debe mostrarse (pts <= reloj). false si no hay uno nuevo.
    bool tomarCuadro(FrameBGRA& out);
    // Último error (vacío si no hubo).
    std::string error() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace engine
