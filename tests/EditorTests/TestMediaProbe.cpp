// TestMediaProbe.cpp
// Escenarios de specs/media-import (análisis y rechazo de archivos).
#include <doctest/doctest.h>

#include "EditorEngine/MediaProbe.h"
#include "MediosPrueba.h"

#include <filesystem>

using engine::MediaProbe;
using model::kMicrosPorSegundo;

TEST_CASE("Video con audio válido") {
    const auto r = MediaProbe::probe(medioPrueba("video_audio.mp4"));
    REQUIRE_MESSAGE(r.ok, r.error);
    CHECK(r.info.hasVideo);
    CHECK(r.info.hasAudio);
    CHECK(r.info.width == 320);
    CHECK(r.info.height == 240);
    CHECK(r.info.fps.toDouble() == doctest::Approx(25.0));
    CHECK(r.info.videoCodec == "mpeg4");
    CHECK(r.info.audioCodec == "aac");
    CHECK(r.info.sampleRate == 48000);
    CHECK(r.info.channels == 1);
    // 5 s (con un margen por el relleno del encoder de audio).
    CHECK(r.info.duration >= 5 * kMicrosPorSegundo - 50'000);
    CHECK(r.info.duration <= 5 * kMicrosPorSegundo + 100'000);
}

TEST_CASE("Video sin audio") {
    const auto r = MediaProbe::probe(medioPrueba("solo_video.mp4"));
    REQUIRE_MESSAGE(r.ok, r.error);
    CHECK(r.info.hasVideo);
    CHECK_FALSE(r.info.hasAudio);
    CHECK(r.info.width == 640);
    CHECK(r.info.height == 360);
    CHECK(r.info.fps.toDouble() == doctest::Approx(30.0));
}

TEST_CASE("Formatos de audio soportados: mp3, wav y aac") {
    for (const char* nombre : {"tono.mp3", "tono.wav", "tono.aac"}) {
        CAPTURE(nombre);
        const auto r = MediaProbe::probe(medioPrueba(nombre));
        REQUIRE_MESSAGE(r.ok, r.error);
        CHECK(r.info.soloAudio());
        CHECK(r.info.sampleRate > 0);
        CHECK(r.info.channels > 0);
        CHECK(r.info.duration > 2 * kMicrosPorSegundo);
    }
}

TEST_CASE("Archivo corrupto o que no es multimedia") {
    const auto r = MediaProbe::probe(medioPrueba("falso.mp4"));
    CHECK_FALSE(r.ok);
    CHECK(r.error.find("falso.mp4") != std::string::npos);  // el mensaje nombra el archivo
}

TEST_CASE("Ruta con tildes y ñ (UTF-8)") {
    // Se copia un medio a una carpeta con caracteres no ASCII y se analiza desde allí.
    const std::string origen = medioPrueba("video_audio.mp4");
    const std::string carpetaUtf8 = medioPrueba("carpeta con ñandú y canción");
    const auto carpeta = std::filesystem::path(std::u8string(carpetaUtf8.begin(), carpetaUtf8.end()));
    std::filesystem::create_directories(carpeta);
    const auto destino = carpeta / L"vídeo.mp4";
    std::filesystem::copy_file(std::filesystem::path(std::u8string(origen.begin(), origen.end())), destino,
                               std::filesystem::copy_options::overwrite_existing);

    const std::u8string u8 = destino.u8string();
    const auto r = MediaProbe::probe(std::string(u8.begin(), u8.end()));
    CHECK_MESSAGE(r.ok, r.error);
    CHECK(r.info.width == 320);
    // El nombre en disco debe ser exactamente el pedido (sin "mojibake").
    CHECK(carpeta.filename().wstring() == L"carpeta con ñandú y canción");
}

TEST_CASE("Archivo inexistente") {
    const auto r = MediaProbe::probe(medioPrueba("no_existe.mp4"));
    CHECK_FALSE(r.ok);
    CHECK(r.error.find("no existe") != std::string::npos);
}
