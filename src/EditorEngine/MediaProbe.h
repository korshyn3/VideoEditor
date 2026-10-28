// MediaProbe.h
// Capa: ENGINE (API pública, sin tipos de FFmpeg).
//
// Abre un archivo multimedia en modo SOLO LECTURA y obtiene su información (duración,
// streams, resolución, fps, códecs...). Si el archivo no se puede usar, devuelve un
// mensaje de error claro que nombra el archivo y la causa.
#pragma once

#include "EditorModel/MediaInfo.h"

#include <string>

namespace engine {

struct ProbeResult {
    bool ok = false;
    model::MediaInfo info;  // válido solo si ok == true
    std::string error;      // mensaje para el usuario si ok == false
};

class MediaProbe {
public:
    // rutaUtf8: ruta del archivo en UTF-8 (FFmpeg acepta UTF-8 en Windows).
    static ProbeResult probe(const std::string& rutaUtf8);
};

} // namespace engine
