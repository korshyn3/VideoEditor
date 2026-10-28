// MediaInfo.h
// Capa: MODEL (C++ estándar; sin FFmpeg ni MFC).
//
// Descripción de un archivo multimedia fuente (video o audio). La llena el engine
// (MediaProbe) al importar, pero es un dato del modelo: se guarda en el proyecto y la
// View la muestra en el panel de propiedades. El archivo original nunca se modifica.
#pragma once

#include "EditorModel/Tiempo.h"

#include <string>

namespace model {

struct MediaInfo {
    std::string path;           // ruta absoluta en UTF-8
    TimeUs duration = 0;        // duración total del archivo

    bool hasVideo = false;
    int width = 0;
    int height = 0;
    Rational fps;               // fotogramas por segundo (p. ej. 30000/1001)
    std::string videoCodec;     // nombre corto del códec ("h264", "mpeg4", ...)

    bool hasAudio = false;
    int sampleRate = 0;         // Hz
    int channels = 0;
    std::string audioCodec;     // "aac", "mp3", "pcm_s16le", ...

    // true si el archivo ya no existe o no se puede abrir (proyecto cargado desde JSON).
    bool offline = false;

    bool soloAudio() const { return hasAudio && !hasVideo; }
};

} // namespace model
