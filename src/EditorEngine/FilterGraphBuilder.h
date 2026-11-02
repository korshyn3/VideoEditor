// FilterGraphBuilder.h
// Capa: ENGINE (API pública, sin tipos de FFmpeg).
//
// Traduce un timeline (una COPIA inmutable: TimelineSnapshot) a una cadena de filtros de
// libavfilter, con la misma sintaxis que `ffmpeg -filter_complex`. La misma cadena se usa
// para el preview y para la exportación: lo que se ve es lo que se exporta.
//
// Ver design.md, D6. Resumen:
//   - cada pista es un `concat` de segmentos (clips y huecos) desde T0 hasta el final;
//   - los segmentos de video se cuantizan a la rejilla de cuadros del proyecto;
//   - el audio (todas las pistas de audio no silenciadas) se mezcla con `amix`;
//   - las salidas se llaman [vout] (video) y [aout] (audio, 48 kHz estéreo float).
// Cada clip aporta una ENTRADA ([in0], [in1], ...) que el GraphRunner alimenta con los
// cuadros de un MediaDecoder.
#pragma once

#include "EditorModel/MediaPool.h"
#include "EditorModel/Project.h"
#include "EditorModel/Timeline.h"

#include <string>
#include <vector>

namespace engine {

// Copia del estado del proyecto que necesita el render (se pasa por valor entre hilos).
struct TimelineSnapshot {
    model::Timeline timeline;
    model::MediaPool media;
    model::CanvasSettings canvas;
};

// Resolución y fps del video que produce el grafo.
struct RenderSettings {
    int width = 0;
    int height = 0;
    model::Rational fps{30, 1};
};

// Una entrada del grafo: un stream de un archivo, limitado a [sourceIn, sourceOut).
struct GraphInput {
    std::string label;      // "in0", "in1", ...
    std::string mediaPath;  // UTF-8
    bool video = true;      // true: stream de video; false: stream de audio
    model::TimeUs sourceIn = 0;
    model::TimeUs sourceOut = 0;
};

struct GraphDescription {
    bool vacio = true;              // timeline sin nada que mostrar en el rango pedido
    std::string filtros;            // cadena filter_complex
    std::vector<GraphInput> inputs; // en el orden de sus etiquetas
    model::TimeUs t0 = 0;           // instante del primer cuadro (alineado a la rejilla)
    model::TimeUs fin = 0;          // instante final (exclusivo)
    RenderSettings salida;
};

class FilterGraphBuilder {
public:
    static constexpr int kSampleRate = 48000;

    // Construye el grafo para mostrar el timeline desde `desde` hasta `hasta`
    // (hasta < 0 = hasta el final del timeline).
    static GraphDescription build(const TimelineSnapshot& snap, const RenderSettings& salida, model::TimeUs desde,
                                  model::TimeUs hasta = -1);

    // Grafo de exportación: el timeline completo (desde 0) a `width` x `height`. El lienzo
    // se escala conservando su relación de aspecto (con barras negras si hace falta) y las
    // salidas se convierten a los formatos que pide el encoder ("yuv420p", "nv12"...;
    // "fltp", "s16"...).
    static GraphDescription buildExport(const TimelineSnapshot& snap, int width, int height,
                                        const std::string& pixFmt, const std::string& sampleFmt);

    // Resolución del preview: la del lienzo reducida a un ancho máximo (dimensiones pares).
    static RenderSettings resolucionPreview(const model::CanvasSettings& canvas, int anchoMaximo);

    // Rejilla de cuadros: índice del cuadro que contiene t, y tiempo de inicio del cuadro n.
    static long long cuadroEn(model::TimeUs t, model::Rational fps);
    static model::TimeUs tiempoDeCuadro(long long n, model::Rational fps);
};

} // namespace engine
