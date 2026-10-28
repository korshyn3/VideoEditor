// Transition.h
// Capa: MODEL.
//
// Transición entre dos clips CONSECUTIVOS de una misma pista de video. Durante
// `duration` ambos clips se solapan (ver specs/clip-transitions).
#pragma once

#include "EditorModel/Ids.h"
#include "EditorModel/Tiempo.h"

namespace model {

enum class TransitionStyle {
    Dissolve,   // fundido
    WipeLeft,   // barrido hacia la izquierda
    WipeRight,  // barrido hacia la derecha
    SlideLeft,  // deslizamiento hacia la izquierda
    SlideRight, // deslizamiento hacia la derecha
    Circle,     // apertura circular
};

struct Transition {
    Id fromClipId = kSinId;  // clip que termina
    Id toClipId = kSinId;    // clip que empieza
    TransitionStyle style = TransitionStyle::Dissolve;
    TimeUs duration = kMicrosPorSegundo;
};

} // namespace model
