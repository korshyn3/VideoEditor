// FFmpegError.cpp
// Capa: ENGINE. Ver FFmpegError.h.
#include "EditorEngine/FFmpegError.h"
#include "EditorEngine/ffmpeg_includes.h"

#include <cerrno>

namespace engine {

std::string mensajeErrorFFmpeg(int codigoError) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {};
    // av_strerror escribe un texto genérico aunque no conozca el código; aun así nos
    // aseguramos de no devolver nunca una cadena vacía.
    av_strerror(codigoError, buf, sizeof(buf));
    std::string texto = buf;
    if (texto.empty())
        texto = "Error desconocido de FFmpeg (" + std::to_string(codigoError) + ")";
    return texto;
}

int codigoErrorArchivoNoEncontrado() {
    return AVERROR(ENOENT);
}

} // namespace engine
