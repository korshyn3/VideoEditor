// TestExport.cpp
// Escenarios de specs/video-export (fase 9): encoders disponibles, validación de la
// salida, exportación real verificada con MediaProbe, cancelación y bloqueo de la edición.
#include <doctest/doctest.h>

#include "EditorController/EditorController.h"
#include "EditorEngine/EncoderCatalog.h"
#include "EditorEngine/ExportJob.h"
#include "EditorEngine/MediaProbe.h"
#include "MediosPrueba.h"

#include <filesystem>

using namespace model;
using engine::EncoderCatalog;

namespace {

TimeUs s(double segundos) { return segundosATiempo(segundos); }

// Carpeta temporal para las salidas de los tests (UTF-8).
std::string salidaTemporal(const std::string& nombre) {
    const auto dir = std::filesystem::temp_directory_path() / "editor_tests";
    std::filesystem::create_directories(dir);
    const std::u8string u = (dir / nombre).u8string();
    return std::string(u.begin(), u.end());
}

std::filesystem::path ruta(const std::string& utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

bool contiene(const std::string& txt, const std::string& sub) { return txt.find(sub) != std::string::npos; }

// Configuración válida pequeña (rápida de exportar).
ExportSettings configuracion(Container c, const std::string& salida) {
    ExportSettings e;
    e.container = c;
    e.videoCodec = "mpeg4";
    e.audioCodec = "aac";
    e.videoBitrateKbps = 1000;
    e.audioBitrateKbps = 128;
    e.preset = ResolutionPreset::Custom;
    e.width = 320;
    e.height = 240;
    e.outputPath = salida;
    return e;
}

} // namespace

TEST_CASE("EncoderCatalog: mpeg4 y aac en los tres contenedores") {
    for (Container c : {Container::Mp4, Container::Mkv, Container::Mov}) {
        CHECK(EncoderCatalog::admite(c, "mpeg4"));
        CHECK(EncoderCatalog::admite(c, "aac"));
        bool hayMpeg4 = false;
        for (const auto& e : EncoderCatalog::para(c, true)) hayMpeg4 = hayMpeg4 || e.nombre == "mpeg4";
        CHECK(hayMpeg4);
        for (const auto& e : EncoderCatalog::para(c, false)) CHECK_FALSE(e.video);
    }
    CHECK_FALSE(EncoderCatalog::admite(Container::Mp4, "no_existe"));
    CHECK(std::string(EncoderCatalog::extension(Container::Mkv)) == ".mkv");
}

TEST_CASE("La extensión corresponde al contenedor") {
    CHECK(controller::EditorController::conExtension("C:/v/salida", Container::Mkv) == "C:/v/salida.mkv");
    CHECK(controller::EditorController::conExtension("C:/v/salida.mp4", Container::Mov) == "C:/v/salida.mov");
}

TEST_CASE("Salida igual a una fuente") {
    controller::EditorController ctrl;
    const std::string fuente = medioPrueba("video_audio.mp4");
    REQUIRE(ctrl.importFiles({fuente}).clips.size() == 1);
    const auto tamano = std::filesystem::file_size(ruta(fuente));
    const OpResult r = ctrl.startExport(configuracion(Container::Mp4, fuente));
    CHECK_FALSE(r.ok);
    CHECK(contiene(r.error, "fuente"));
    CHECK_FALSE(ctrl.exportando());
    CHECK(std::filesystem::file_size(ruta(fuente)) == tamano);  // no se tocó
}

TEST_CASE("Resolución impar") {
    controller::EditorController ctrl;
    REQUIRE(ctrl.importFiles({medioPrueba("video_audio.mp4")}).clips.size() == 1);
    ExportSettings e = configuracion(Container::Mkv, salidaTemporal("impar"));
    e.width = 1279;
    e.height = 719;
    const OpResult r = ctrl.validarExportacion(e);
    CHECK_FALSE(r.ok);
    CHECK(contiene(r.error, "pares"));
    e.width = 1278;
    e.height = 718;
    CHECK(ctrl.validarExportacion(e).ok);
}

TEST_CASE("Valores de exportación fuera de rango") {
    controller::EditorController ctrl;
    ExportSettings e = configuracion(Container::Mkv, salidaTemporal("rango"));
    CHECK_FALSE(ctrl.validarExportacion(e).ok);  // timeline vacío
    REQUIRE(ctrl.importFiles({medioPrueba("video_audio.mp4")}).clips.size() == 1);
    CHECK(ctrl.validarExportacion(e).ok);
    e.videoBitrateKbps = 50;
    CHECK_FALSE(ctrl.validarExportacion(e).ok);
    e.videoBitrateKbps = 5000;
    e.audioBitrateKbps = 600;
    CHECK_FALSE(ctrl.validarExportacion(e).ok);
    e.audioBitrateKbps = 192;
    e.width = 8000;
    CHECK_FALSE(ctrl.validarExportacion(e).ok);
    e.preset = ResolutionPreset::P720;  // el preset manda sobre la personalizada
    CHECK(ctrl.validarExportacion(e).ok);
    e.outputPath.clear();
    CHECK_FALSE(ctrl.validarExportacion(e).ok);
}

TEST_CASE("Exportar 5 s a mkv con mpeg4 y aac") {
    controller::EditorController ctrl;
    REQUIRE(ctrl.importFiles({medioPrueba("video_audio.mp4")}).clips.size() == 1);
    const std::string salida = salidaTemporal("exportado.mkv");
    std::filesystem::remove(ruta(salida));
    REQUIRE(ctrl.startExport(configuracion(Container::Mkv, salida)));
    ctrl.esperarExport();
    const engine::ExportResult r = ctrl.resultadoExport();
    REQUIRE_MESSAGE(r.estado == engine::ExportEstado::Terminado, r.error);
    CHECK(ctrl.progresoExport() == 1.0);
    CHECK(ctrl.project().exportSettings().outputPath == salida);  // se recuerda en el proyecto

    const engine::ProbeResult p = engine::MediaProbe::probe(salida);
    REQUIRE_MESSAGE(p.ok, p.error);
    CHECK(p.info.duration == doctest::Approx(static_cast<double>(s(5))).epsilon(0.02));
    CHECK(p.info.width == 320);
    CHECK(p.info.height == 240);
    CHECK(p.info.hasVideo);
    CHECK(p.info.hasAudio);
    CHECK(p.info.videoCodec == "mpeg4");
    CHECK(p.info.audioCodec == "aac");
    CHECK(p.info.sampleRate == 48000);
    CHECK(p.info.channels == 2);
}

TEST_CASE("Preset 720p con barras negras") {
    controller::EditorController ctrl;  // lienzo 320x240 (4:3)
    REQUIRE(ctrl.importFiles({medioPrueba("solo_video.mp4")}).clips.size() == 1);
    ExportSettings e = configuracion(Container::Mp4, salidaTemporal("p720.mp4"));
    e.preset = ResolutionPreset::P720;
    REQUIRE(ctrl.startExport(e));
    ctrl.esperarExport();
    REQUIRE_MESSAGE(ctrl.resultadoExport().estado == engine::ExportEstado::Terminado, ctrl.resultadoExport().error);
    const engine::ProbeResult p = engine::MediaProbe::probe(salidaTemporal("p720.mp4"));
    REQUIRE_MESSAGE(p.ok, p.error);
    CHECK(p.info.width == 1280);
    CHECK(p.info.height == 720);
    CHECK(p.info.duration == doctest::Approx(static_cast<double>(s(3))).epsilon(0.03));
    CHECK(p.info.hasAudio);  // silencio, pero con stream de audio
}

TEST_CASE("Encoder falla al iniciar") {
    controller::EditorController ctrl;
    REQUIRE(ctrl.importFiles({medioPrueba("video_audio.mp4")}).clips.size() == 1);
    engine::ExportRequest pedido;
    pedido.snap = {ctrl.project().timeline(), ctrl.project().media(), ctrl.project().canvas()};
    pedido.rutaSalida = salidaTemporal("falla.mp4");
    std::filesystem::remove(ruta(pedido.rutaSalida));
    pedido.encoderVideo = "no_existe";
    pedido.width = 320;
    pedido.height = 240;
    std::stop_source fuente;
    const engine::ExportResult r = engine::ExportJob::exportar(pedido, fuente.get_token(), nullptr);
    CHECK(r.estado == engine::ExportEstado::Error);
    CHECK(contiene(r.error, "no_existe"));  // nombra el códec
    CHECK_FALSE(std::filesystem::exists(ruta(pedido.rutaSalida)));
}

TEST_CASE("Cancelar borra el archivo parcial y la edición queda bloqueada mientras tanto") {
    controller::EditorController ctrl;
    REQUIRE(ctrl.importFiles({medioPrueba("sincronia.mp4"), medioPrueba("sincronia.mp4")}).clips.size() == 2);
    ExportSettings e = configuracion(Container::Mp4, salidaTemporal("cancelado.mp4"));
    e.preset = ResolutionPreset::P1080;  // 20 s a 1080p: tarda lo suficiente para cancelar
    REQUIRE(ctrl.startExport(e));
    const auto rev = ctrl.project().revision();
    if (ctrl.exportando()) {
        CHECK_FALSE(ctrl.addTrack(TrackKind::Video).ok);
        CHECK_FALSE(ctrl.importFiles({medioPrueba("tono.wav")}).errores.empty());
        CHECK(ctrl.project().revision() == rev);
    }
    ctrl.cancelExport();
    ctrl.esperarExport();
    CHECK(ctrl.resultadoExport().estado == engine::ExportEstado::Cancelado);
    CHECK_FALSE(std::filesystem::exists(ruta(salidaTemporal("cancelado.mp4"))));
    CHECK(ctrl.addTrack(TrackKind::Video).ok);  // la edición vuelve a estar habilitada
}

// Herramienta (omitida por defecto) para la tarea 9.5: exporta 10 s con h264_mf a 2000
// kbps y deja el archivo en la carpeta temporal para medirlo con ffprobe. Uso:
//   EditorTests.exe -tc="Medir h264_mf" --no-skip
TEST_CASE("Medir h264_mf" * doctest::skip()) {
    REQUIRE(EncoderCatalog::admite(Container::Mp4, "h264_mf"));
    controller::EditorController ctrl;
    REQUIRE(ctrl.importFiles({medioPrueba("video_audio.mp4"), medioPrueba("video_audio.mp4")}).clips.size() == 2);
    ExportSettings e = configuracion(Container::Mp4, salidaTemporal("h264_mf_2000.mp4"));
    e.videoCodec = "h264_mf";
    e.videoBitrateKbps = 2000;
    e.preset = ResolutionPreset::P720;
    REQUIRE(ctrl.startExport(e));
    ctrl.esperarExport();
    REQUIRE_MESSAGE(ctrl.resultadoExport().estado == engine::ExportEstado::Terminado, ctrl.resultadoExport().error);
    MESSAGE("Exportado: " << salidaTemporal("h264_mf_2000.mp4"));
}
