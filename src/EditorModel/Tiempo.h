// Tiempo.h
// Capa: MODEL (C++ estándar; sin FFmpeg ni MFC).
//
// Representación del tiempo en todo el modelo. Se usan enteros de 64 bits en
// MICROSEGUNDOS en lugar de double para que las operaciones de edición (split, trim,
// transiciones) no acumulen errores de redondeo. Solo el engine convierte a las bases
// de tiempo de FFmpeg (AVRational).
#pragma once

#include <cstdint>
#include <string>

namespace model {

// Tiempo en microsegundos (1 s = 1'000'000).
using TimeUs = std::int64_t;

constexpr TimeUs kMicrosPorSegundo = 1'000'000;

constexpr TimeUs segundosATiempo(double segundos) {
    return static_cast<TimeUs>(segundos * static_cast<double>(kMicrosPorSegundo) + (segundos >= 0 ? 0.5 : -0.5));
}

constexpr double tiempoASegundos(TimeUs t) {
    return static_cast<double>(t) / static_cast<double>(kMicrosPorSegundo);
}

// Fracción entera (por ejemplo 30000/1001 fps). Evita perder precisión con fps NTSC.
struct Rational {
    int num = 0;
    int den = 1;

    double toDouble() const { return den != 0 ? static_cast<double>(num) / den : 0.0; }
    bool valido() const { return num > 0 && den > 0; }
};

// Formatea un tiempo como "h:mm:ss.mmm" (o "m:ss.mmm" si dura menos de una hora).
std::string formatearTiempo(TimeUs t);

} // namespace model
