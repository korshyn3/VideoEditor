// Effect.h
// Capa: MODEL.
//
// Efectos de video aplicables a un clip. Cada efecto es un struct con sus parámetros y
// Effect es un std::variant: un valor que es exactamente UNO de esos tipos. El engine
// traduce cada uno a un filtro de libavfilter (ver design.md, D6).
#pragma once

#include "EditorModel/Tiempo.h"

#include <variant>

namespace model {

// Elimina la saturación de color (blanco y negro).
struct Grayscale {};

// brightness: -1.0 .. 1.0 (0 = sin cambio); contrast: 0.0 .. 3.0 (1 = sin cambio).
struct BrightnessContrast {
    double brightness = 0.0;
    double contrast = 1.0;
};

// radius: 0 .. 50 (0 = sin desenfoque).
struct Blur {
    double radius = 5.0;
};

// Fundido desde/hacia negro al inicio/fin del clip.
struct Fade {
    TimeUs fadeIn = kMicrosPorSegundo;
    TimeUs fadeOut = 0;
};

using Effect = std::variant<Grayscale, BrightnessContrast, Blur, Fade>;

} // namespace model
