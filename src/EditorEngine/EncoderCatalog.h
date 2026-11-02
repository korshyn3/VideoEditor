// EncoderCatalog.h
// Capa: ENGINE (API pública, sin tipos de FFmpeg).
//
// Qué encoders se pueden usar para exportar EN ESTE EQUIPO y en qué contenedores. Un
// encoder puede estar en la DLL de FFmpeg y aun así fallar (los de MediaFoundation
// dependen del hardware), así que cada candidato se prueba abriéndolo con un
// avcodec_open2 de prueba (64x64). La compatibilidad con el contenedor se consulta con
// avformat_query_codec. Ver design.md, D9.
#pragma once

#include "EditorModel/ExportSettings.h"

#include <string>
#include <vector>

namespace engine {

struct EncoderInfo {
    std::string nombre;       // nombre del encoder en FFmpeg ("h264_mf", "mpeg4", "aac"...)
    std::string descripcion;  // para la interfaz ("H.264 (MediaFoundation)")
    bool video = true;
};

class EncoderCatalog {
public:
    // Candidatos que abrieron bien (se prueban una sola vez; la primera llamada tarda).
    static const std::vector<EncoderInfo>& disponibles();
    // Los disponibles de un tipo (video o audio) que el contenedor admite.
    static std::vector<EncoderInfo> para(model::Container contenedor, bool video);
    // true si el encoder está disponible y el contenedor lo admite.
    static bool admite(model::Container contenedor, const std::string& encoder);

    // Nombre del formato de FFmpeg ("mp4", "matroska", "mov") y extensión (".mp4"...).
    static const char* formato(model::Container contenedor);
    static const char* extension(model::Container contenedor);
};

} // namespace engine
