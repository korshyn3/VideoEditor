// Clip.h
// Capa: MODEL.
//
// Un clip es una REFERENCIA a un tramo de un archivo fuente colocado en el timeline.
// Nunca contiene ni modifica los datos del archivo (edición no destructiva).
//
// Un clip de video con audio tiene un clip de audio VINCULADO (linkedClipId, en los dos
// sentidos) en la pista de audio de su pista de video. Mientras están vinculados tienen
// el mismo medio, tramo y posición, y las operaciones de edición se aplican a los dos.
//
//   archivo fuente:  |----[sourceIn ======= sourceOut]------|
//   timeline:              [timelineStart ===== timelineEnd()]
#pragma once

#include "EditorModel/Effect.h"
#include "EditorModel/Ids.h"
#include "EditorModel/Tiempo.h"

#include <vector>

namespace model {

// Transformación de un clip superpuesto (picture-in-picture) en pistas superiores a V1.
// x, y: esquina superior izquierda relativa al lienzo (0..1); scale: fracción del ancho
// del lienzo (0.05..1); opacity: 0..1.
struct PipTransform {
    static constexpr double kEscalaMin = 0.05;
    double x = 0.65;
    double y = 0.65;
    double scale = 0.3;
    double opacity = 1.0;
    bool operator==(const PipTransform&) const = default;
};

struct Clip {
    Id id = kSinId;
    Id mediaId = kSinId;         // medio fuente (MediaPool)
    TimeUs sourceIn = 0;         // punto de entrada dentro del archivo fuente
    TimeUs sourceOut = 0;        // punto de salida (exclusivo)
    TimeUs timelineStart = 0;    // dónde empieza en el timeline
    Id linkedClipId = kSinId;    // clip vinculado (video <-> su audio), o ninguno
    std::vector<Effect> effects; // se aplican en este orden
    PipTransform transform;

    TimeUs duration() const { return sourceOut - sourceIn; }
    TimeUs timelineEnd() const { return timelineStart + duration(); }
    // true si el instante t del timeline cae dentro del clip.
    bool contiene(TimeUs t) const { return t >= timelineStart && t < timelineEnd(); }
    bool vinculado() const { return linkedClipId != kSinId; }
};

} // namespace model
