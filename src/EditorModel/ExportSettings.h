// ExportSettings.h
// Capa: MODEL.
//
// Última configuración de exportación elegida por el usuario (se guarda con el proyecto).
// Los valores válidos se describen en specs/video-export.
#pragma once

#include <string>

namespace model {

enum class Container { Mp4, Mkv, Mov };

enum class ResolutionPreset { P480, P720, P1080, Custom };

struct ExportSettings {
    Container container = Container::Mp4;
    std::string videoCodec = "h264";  // nombre de códec ("h264", "hevc", "mpeg4")
    std::string audioCodec = "aac";
    int videoBitrateKbps = 5000;      // 100 .. 100000
    int audioBitrateKbps = 192;       // 32 .. 512
    ResolutionPreset preset = ResolutionPreset::P1080;
    int width = 1920;                 // usado si preset == Custom
    int height = 1080;
    std::string outputPath;           // UTF-8
};

} // namespace model
