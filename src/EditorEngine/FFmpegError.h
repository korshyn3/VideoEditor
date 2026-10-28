// FFmpegError.h
// Capa: ENGINE (API pública, sin tipos de FFmpeg).
//
// Traducción de los códigos de error de FFmpeg (valores AVERROR negativos) a texto
// legible para mostrar al usuario.
#pragma once

#include <string>

namespace engine {

// Devuelve la descripción de un código de error de FFmpeg (p. ej. "No such file or
// directory"). Nunca devuelve una cadena vacía.
std::string mensajeErrorFFmpeg(int codigoError);

// Código AVERROR(ENOENT) expuesto sin incluir FFmpeg (útil en tests).
int codigoErrorArchivoNoEncontrado();

} // namespace engine
