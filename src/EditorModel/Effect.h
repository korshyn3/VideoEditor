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
struct Grayscale {
    bool operator==(const Grayscale&) const = default;
};

// brightness: -1.0 .. 1.0 (0 = sin cambio); contrast: 0.0 .. 3.0 (1 = sin cambio).
struct BrightnessContrast {
    static constexpr double kBrilloMin = -1.0, kBrilloMax = 1.0;
    static constexpr double kContrasteMin = 0.0, kContrasteMax = 3.0;
    double brightness = 0.0;
    double contrast = 1.0;
    bool operator==(const BrightnessContrast&) const = default;
};

// radius: 0 .. 50 (0 = sin desenfoque).
struct Blur {
    static constexpr double kRadioMax = 50.0;
    double radius = 5.0;
    bool operator==(const Blur&) const = default;
};

// Fundido desde/hacia negro al inicio/fin del clip. La suma de los dos no puede superar
// la duración del clip.
struct Fade {
    TimeUs fadeIn = kMicrosPorSegundo;
    TimeUs fadeOut = 0;
    bool operator==(const Fade&) const = default;
};

using Effect = std::variant<Grayscale, BrightnessContrast, Blur, Fade>;

// Tipos de efecto en el orden del variant (para la interfaz: índice = tipo).
inline constexpr int kTiposEfecto = static_cast<int>(std::variant_size_v<Effect>);

// Efecto nuevo del tipo dado (índice del variant) con sus valores por defecto.
Effect efectoPorDefecto(int tipo);

// Nombre para la interfaz ("Escala de grises", "Brillo/contraste", ...).
const char* nombreEfecto(int tipo);
inline const char* nombreEfecto(const Effect& e) { return nombreEfecto(static_cast<int>(e.index())); }

// Copia del efecto con los parámetros limitados a sus rangos (los fades, a >= 0).
Effect limitarEfecto(Effect e);

} // namespace model
