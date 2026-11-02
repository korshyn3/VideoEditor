// Effect.cpp
// Capa: MODEL. Ver Effect.h.
#include "EditorModel/Effect.h"

#include <algorithm>

namespace model {

namespace {

// Ayuda para std::visit: un objeto con un operator() por cada tipo del variant.
template <class... F>
struct Sobrecargas : F... {
    using F::operator()...;
};

} // namespace

Effect efectoPorDefecto(int tipo) {
    switch (tipo) {
    case 1: return BrightnessContrast{};
    case 2: return Blur{};
    case 3: return Fade{};
    default: return Grayscale{};
    }
}

const char* nombreEfecto(int tipo) {
    switch (tipo) {
    case 0: return "Escala de grises";
    case 1: return "Brillo/contraste";
    case 2: return "Desenfoque";
    case 3: return "Fade in/out";
    default: return "?";
    }
}

Effect limitarEfecto(Effect e) {
    std::visit(Sobrecargas{
                   [](Grayscale&) {},
                   [](BrightnessContrast& b) {
                       b.brightness = std::clamp(b.brightness, BrightnessContrast::kBrilloMin, BrightnessContrast::kBrilloMax);
                       b.contrast = std::clamp(b.contrast, BrightnessContrast::kContrasteMin, BrightnessContrast::kContrasteMax);
                   },
                   [](Blur& b) { b.radius = std::clamp(b.radius, 0.0, Blur::kRadioMax); },
                   [](Fade& f) {
                       f.fadeIn = std::max<TimeUs>(0, f.fadeIn);
                       f.fadeOut = std::max<TimeUs>(0, f.fadeOut);
                   },
               },
               e);
    return e;
}

} // namespace model
