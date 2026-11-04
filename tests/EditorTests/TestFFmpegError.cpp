// TestFFmpegError.cpp
// Capa: TESTS. Traducción de códigos AVERROR a texto (EditorEngine/FFmpegError).
#include <doctest/doctest.h>

#include "EditorEngine/FFmpegError.h"

TEST_CASE("mensajeErrorFFmpeg traduce AVERROR(ENOENT) a texto") {
    const std::string msg = engine::mensajeErrorFFmpeg(engine::codigoErrorArchivoNoEncontrado());
    CHECK_FALSE(msg.empty());
}

TEST_CASE("mensajeErrorFFmpeg nunca devuelve una cadena vacía") {
    CHECK_FALSE(engine::mensajeErrorFFmpeg(-123456789).empty());
}
