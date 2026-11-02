// GraphRunner.h
// Capa: ENGINE (interno: usa tipos de FFmpeg).
//
// Ejecuta una GraphDescription: crea el grafo de libavfilter, conecta cada entrada
// ([in0], [in1], ...) a un `buffer`/`abuffer` alimentado por un MediaDecoder, y entrega
// los cuadros de las salidas [vout] (video) y [aout] (audio).
//
// El grafo funciona "a demanda": al pedir un cuadro a una salida, libavfilter indica qué
// entradas necesitan datos (av_buffersrc_get_nb_failed_requests) y solo esas se decodifican.
// Gracias a `concat`, en cada pista solo trabaja el decoder del clip actual.
#pragma once

#include "EditorEngine/FilterGraphBuilder.h"
#include "EditorEngine/MediaDecoder.h"
#include "EditorEngine/ffmpeg_includes.h"

#include <memory>
#include <string>
#include <vector>

namespace engine {

class GraphRunner {
public:
    static constexpr int kMuestrasPorBloque = 960;  // 20 ms de audio a 48 kHz

    GraphRunner() = default;
    ~GraphRunner();
    GraphRunner(const GraphRunner&) = delete;
    GraphRunner& operator=(const GraphRunner&) = delete;

    bool build(const GraphDescription& descripcion, std::string* error);
    // Tamaño de los bloques de audio (llamar antes de build). La exportación usa el
    // tamaño de cuadro del encoder (1024 en AAC).
    void setTamanoBloqueAudio(int muestras) { bloqueAudio_ = muestras; }

    // Siguiente cuadro de video (yuv420p) / bloque de audio (float intercalado, estéreo,
    // 48 kHz, kMuestrasPorBloque muestras salvo el último). `pts` en tiempo del timeline.
    //   1 = cuadro, 0 = fin, <0 = error de FFmpeg.
    int pullVideo(AVFrame* out, model::TimeUs* pts);
    int pullAudio(AVFrame* out, model::TimeUs* pts);

    const std::string& ultimoError() const { return error_; }

private:
    struct Entrada {
        std::unique_ptr<MediaDecoder> decoder;
        AVFilterContext* fuente = nullptr;
        bool fin = false;
    };

    int pull(AVFilterContext* sink, AVFrame* out, model::TimeUs* pts);
    // Empuja un cuadro a la entrada que más lo necesita. <0 si no se pudo avanzar.
    int alimentar();

    AVFilterGraph* grafo_ = nullptr;
    AVFilterContext* sinkVideo_ = nullptr;
    AVFilterContext* sinkAudio_ = nullptr;
    std::vector<Entrada> entradas_;
    AVFrame* temporal_ = nullptr;
    std::string error_;
    int bloqueAudio_ = kMuestrasPorBloque;
};

} // namespace engine
