// Tiempo.cpp
// Capa: MODEL. Utilidades de tiempo (ver Tiempo.h).
#include "EditorModel/Tiempo.h"

#include <cstdio>

namespace model {

std::string formatearTiempo(TimeUs t) {
    const bool negativo = t < 0;
    if (negativo) t = -t;

    const TimeUs totalMs = t / 1000;
    const long long ms = totalMs % 1000;
    const long long totalSeg = totalMs / 1000;
    const long long seg = totalSeg % 60;
    const long long min = (totalSeg / 60) % 60;
    const long long horas = totalSeg / 3600;

    char buf[48];
    if (horas > 0)
        std::snprintf(buf, sizeof(buf), "%s%lld:%02lld:%02lld.%03lld", negativo ? "-" : "", horas, min, seg, ms);
    else
        std::snprintf(buf, sizeof(buf), "%s%lld:%02lld.%03lld", negativo ? "-" : "", min, seg, ms);
    return buf;
}

} // namespace model
