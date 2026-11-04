// TestPersistencia.cpp
// Capa: TESTS.
// Escenarios de specs/project-persistence (fase 10): guardar en JSON, ida y vuelta
// (conservando vínculos, efectos, PiP y transiciones), archivos inválidos y medios que
// faltan al abrir.
#include <doctest/doctest.h>

#include "EditorController/EditorController.h"
#include "EditorEngine/FilterGraphBuilder.h"
#include "EditorModel/ProjectSerializer.h"
#include "MediosPrueba.h"

#include <filesystem>
#include <fstream>

using namespace model;

namespace {

TimeUs s(double segundos) { return segundosATiempo(segundos); }

MediaInfo videoFalso(const std::string& nombre, double segundos, bool audio = true) {
    MediaInfo m;
    m.path = "C:/medios/" + nombre;
    m.duration = s(segundos);
    m.hasVideo = true;
    m.width = 1280;
    m.height = 720;
    m.fps = {25, 1};
    m.videoCodec = "h264";
    m.hasAudio = audio;
    m.sampleRate = audio ? 48000 : 0;
    m.channels = audio ? 2 : 0;
    m.audioCodec = audio ? "aac" : "";
    return m;
}

MediaInfo musicaFalsa(double segundos) {
    MediaInfo m;
    m.path = "C:/medios/música ñ.mp3";  // con caracteres no ASCII (UTF-8)
    m.duration = s(segundos);
    m.hasAudio = true;
    m.sampleRate = 44100;
    m.channels = 2;
    m.audioCodec = "mp3";
    return m;
}

// Dos pistas de video, efectos, PiP, una transición, música y un audio desvinculado.
Project proyectoCompleto() {
    Project p;
    const Id v1 = p.timeline().firstTrack(TrackKind::Video)->id;
    Id v2 = kSinId, a = kSinId, b = kSinId, c = kSinId, musica = kSinId;
    p.addTrack(TrackKind::Video, &v2);
    p.appendMedia(v1, p.addMedia(videoFalso("A.mp4", 10)), &a);
    p.appendMedia(v1, p.addMedia(videoFalso("B.mp4", 5)), &b);
    p.appendMedia(v2, p.addMedia(videoFalso("C.mp4", 4, false)), &c);
    p.appendMedia(p.timeline().firstGeneralAudioTrack()->id, p.addMedia(musicaFalsa(20)), &musica);
    REQUIRE(p.addEffect(a, Grayscale{}));
    REQUIRE(p.addEffect(a, Fade{s(1), s(0.5)}));
    REQUIRE(p.addEffect(b, BrightnessContrast{0.25, 1.5}));
    REQUIRE(p.addEffect(c, Blur{7.5}));
    REQUIRE(p.setPipTransform(c, {0.1, 0.2, 0.4, 0.75}));
    REQUIRE(p.addTransition(a, TransitionStyle::Circle, s(1.5)));
    REQUIRE(p.setTrackMuted(p.timeline().firstGeneralAudioTrack()->id, true));
    ExportSettings e;
    e.container = Container::Mkv;
    e.preset = ResolutionPreset::Custom;
    e.width = 640;
    e.height = 360;
    e.outputPath = "C:/salidas/final.mkv";
    p.setExportSettings(e);
    return p;
}

bool contiene(const std::string& txt, const std::string& sub) { return txt.find(sub) != std::string::npos; }

std::string temporal(const std::string& nombre) {
    const auto dir = std::filesystem::temp_directory_path() / "editor_tests";
    std::filesystem::create_directories(dir);
    const std::u8string u = (dir / nombre).u8string();
    return std::string(u.begin(), u.end());
}

std::filesystem::path ruta(const std::string& utf8) { return std::filesystem::path(std::u8string(utf8.begin(), utf8.end())); }

void escribir(const std::string& rutaUtf8, const std::string& texto) {
    std::ofstream(ruta(rutaUtf8), std::ios::binary) << texto;
}

} // namespace

TEST_CASE("Guardar") {
    const Project p = proyectoCompleto();
    const std::string j = ProjectSerializer::toJson(p);
    CHECK(contiene(j, "\"formatVersion\": 2"));
    CHECK(contiene(j, "\"canvas\""));
    CHECK(contiene(j, "C:/medios/A.mp4"));
    CHECK(contiene(j, "\"style\": \"circle\""));
    CHECK(contiene(j, "\"type\": \"grayscale\""));
    CHECK(contiene(j, "\"type\": \"brightnessContrast\""));
    CHECK(contiene(j, "\"linkedClipId\""));
    CHECK(contiene(j, "\"videoTrackId\""));
    CHECK(contiene(j, "\"opacity\": 0.75"));
    CHECK(contiene(j, "\"outputPath\": \"C:/salidas/final.mkv\""));
    CHECK(contiene(j, "\n  "));  // con sangría: legible
}

TEST_CASE("Ida y vuelta (conserva los vínculos)") {
    const Project original = proyectoCompleto();
    const std::string j = ProjectSerializer::toJson(original);
    Project cargado;
    std::string error;
    REQUIRE_MESSAGE(ProjectSerializer::fromJson(j, cargado, &error), error);
    CHECK(ProjectSerializer::toJson(cargado) == j);  // idéntico
    CHECK_FALSE(cargado.dirty());

    // Los vínculos, las pistas de audio de video y las transiciones están.
    const Track* v1 = cargado.timeline().firstTrack(TrackKind::Video);
    REQUIRE(v1->clips.size() == 2);
    const Clip& a = v1->clips[0];
    REQUIRE(a.vinculado());
    const Track* pistaAudio = nullptr;
    REQUIRE(cargado.timeline().findClip(a.linkedClipId, &pistaAudio));
    CHECK(pistaAudio->videoTrackId == v1->id);
    REQUIRE(cargado.transitionFrom(a.id));
    CHECK(cargado.transitionFrom(a.id)->style == TransitionStyle::Circle);
    CHECK(cargado.transitionFrom(a.linkedClipId));  // la espejo del audio
    CHECK(cargado.exportSettings().container == Container::Mkv);
    CHECK(cargado.canvas().width == original.canvas().width);

    // Los ids nuevos no chocan con los cargados.
    Id nuevo = kSinId;
    REQUIRE(cargado.addTrack(TrackKind::Audio, &nuevo));
    int iguales = 0;
    for (const Track& t : cargado.timeline().tracks) {
        iguales += (t.id == nuevo);
        for (const Clip& c : t.clips) iguales += (c.id == nuevo);
    }
    CHECK(iguales == 1);
}

TEST_CASE("JSON corrupto") {
    Project actual = proyectoCompleto();
    const std::string antes = ProjectSerializer::toJson(actual);
    const std::string truncado = antes.substr(0, antes.size() / 2);
    std::string error;
    CHECK_FALSE(ProjectSerializer::fromJson(truncado, actual, &error));
    CHECK_FALSE(error.empty());
    CHECK(ProjectSerializer::toJson(actual) == antes);  // el proyecto no cambió
}

TEST_CASE("Archivos de proyecto inválidos") {
    const std::string bueno = ProjectSerializer::toJson(proyectoCompleto());
    auto rechaza = [](std::string texto) {
        Project p;
        std::string error;
        const bool ok = ProjectSerializer::fromJson(texto, p, &error);
        CHECK_FALSE(error.empty());
        return !ok;
    };
    auto reemplazar = [&](const std::string& de, const std::string& a) {
        std::string t = bueno;
        const auto pos = t.find(de);
        REQUIRE(pos != std::string::npos);
        t.replace(pos, de.size(), a);
        return t;
    };
    CHECK(rechaza(reemplazar("\"formatVersion\": 2", "\"formatVersion\": 7")));  // versión no soportada
    CHECK(rechaza(reemplazar("\"formatVersion\": 2,", "")));                     // falta un campo
    CHECK(rechaza(reemplazar("\"type\": \"grayscale\"", "\"type\": \"sepia\"")));
    CHECK(rechaza(reemplazar("\"style\": \"circle\"", "\"style\": \"estrella\"")));
    CHECK(rechaza("[1, 2, 3]"));
    CHECK(rechaza(""));
}

TEST_CASE("Abrir un proyecto de la versión 1: los clips de V1 ocupan todo el lienzo") {
    Project p;
    Id v2 = kSinId, a = kSinId, b = kSinId;
    p.addTrack(TrackKind::Video, &v2);
    p.appendMedia(p.timeline().firstTrack(TrackKind::Video)->id, p.addMedia(videoFalso("A.mp4", 5)), &a);
    p.appendMedia(v2, p.addMedia(videoFalso("B.mp4", 5)), &b);
    // Lo que guardaba la versión 1: el mismo valor en V1 (sin usar) y en V2 (en uso).
    const PipTransform antiguo{0.65, 0.65, 0.3, 1.0};
    REQUIRE(p.setPipTransform(a, antiguo));
    REQUIRE(p.setPipTransform(b, antiguo));

    std::string texto = ProjectSerializer::toJson(p);
    const std::string actual = "\"formatVersion\": 2";
    const auto pos = texto.find(actual);
    REQUIRE(pos != std::string::npos);
    texto.replace(pos, actual.size(), "\"formatVersion\": 1");

    Project cargado;
    std::string error;
    REQUIRE_MESSAGE(ProjectSerializer::fromJson(texto, cargado, &error), error);
    CHECK(cargado.timeline().findClip(a)->transform == PipTransform{});
    CHECK(cargado.timeline().findClip(b)->transform == antiguo);
}

TEST_CASE("Abrir un proyecto con el nombre anterior de las pistas de audio de video") {
    Project p;
    std::string texto = ProjectSerializer::toJson(p);
    const auto pos = texto.find("\"AV1\"");
    REQUIRE(pos != std::string::npos);
    texto.replace(pos, 5, "\"Audio V1\"");

    Project cargado;
    std::string error;
    REQUIRE_MESSAGE(ProjectSerializer::fromJson(texto, cargado, &error), error);
    CHECK(cargado.timeline().tracks[1].name == "AV1");
}

TEST_CASE("Vínculos o transiciones incoherentes se rechazan") {
    Project p;
    const Id v1 = p.timeline().firstTrack(TrackKind::Video)->id;
    Id a = kSinId, b = kSinId;
    p.appendMedia(v1, p.addMedia(videoFalso("A.mp4", 10)), &a);
    p.appendMedia(v1, p.addMedia(videoFalso("B.mp4", 5)), &b);
    REQUIRE(p.addTransition(a, TransitionStyle::Dissolve, s(1)));
    const std::string bueno = ProjectSerializer::toJson(p);
    Project q;
    std::string error;
    REQUIRE(ProjectSerializer::fromJson(bueno, q, &error));

    // B empieza 1 s más tarde: ya no se solapa con A como dice su transición.
    std::string t = bueno;
    const std::string inicioB = "\"startUs\": 9000000";
    size_t pos = t.find(inicioB);
    REQUIRE(pos != std::string::npos);
    t.replace(pos, inicioB.size(), "\"startUs\": 9500000");
    CHECK_FALSE(ProjectSerializer::fromJson(t, q, &error));

    // Un clip vinculado a un clip que no existe.
    t = bueno;
    const std::string vinculo = "\"linkedClipId\": " + std::to_string(p.timeline().findClip(a)->linkedClipId);
    pos = t.find(vinculo);
    REQUIRE(pos != std::string::npos);
    t.replace(pos, vinculo.size(), "\"linkedClipId\": 999");
    CHECK_FALSE(ProjectSerializer::fromJson(t, q, &error));
    CHECK(contiene(error, "vínculo"));
}

TEST_CASE("Controller: guardar y abrir") {
    controller::EditorController ctrl;
    REQUIRE(ctrl.importFiles({medioPrueba("video_audio.mp4"), medioPrueba("tono.wav")}).clips.size() == 2);
    CHECK(ctrl.project().dirty());
    const std::string archivo = temporal("proyecto.json");
    REQUIRE(ctrl.guardar(archivo));
    CHECK_FALSE(ctrl.project().dirty());
    CHECK(ctrl.rutaProyecto() == archivo);
    const std::string guardado = ProjectSerializer::toJson(ctrl.project());

    controller::EditorController otro;
    const controller::CargaResult r = otro.abrir(archivo);
    REQUIRE_MESSAGE(r.r.ok, r.r.error);
    CHECK(r.faltantes.empty());
    CHECK(ProjectSerializer::toJson(otro.project()) == guardado);
    CHECK_FALSE(otro.project().dirty());

    // Un archivo corrupto no cambia el proyecto abierto.
    escribir(temporal("corrupto.json"), guardado.substr(0, 100));
    const controller::CargaResult malo = otro.abrir(temporal("corrupto.json"));
    CHECK_FALSE(malo.r.ok);
    CHECK(ProjectSerializer::toJson(otro.project()) == guardado);
    CHECK(otro.rutaProyecto() == archivo);
}

TEST_CASE("Archivo movido") {
    // B.mp4 es una copia de un medio de prueba que se borra después de guardar.
    const std::string b = temporal("B.mp4");
    std::filesystem::copy_file(ruta(medioPrueba("video_audio.mp4")), ruta(b),
                               std::filesystem::copy_options::overwrite_existing);
    controller::EditorController ctrl;
    REQUIRE(ctrl.importFiles({medioPrueba("solo_video.mp4"), b}).clips.size() == 2);
    const std::string archivo = temporal("con_b.json");
    REQUIRE(ctrl.guardar(archivo));
    std::filesystem::remove(ruta(b));

    controller::EditorController otro;
    const controller::CargaResult r = otro.abrir(archivo);
    REQUIRE_MESSAGE(r.r.ok, r.r.error);  // el proyecto se abre igual
    REQUIRE(r.faltantes.size() == 1);
    CHECK(r.faltantes[0] == b);
    int offline = 0;
    for (const auto& [id, info] : otro.project().media().items()) offline += info.offline;
    CHECK(offline == 1);
    CHECK(otro.project().timeline().firstTrack(TrackKind::Video)->clips.size() == 2);  // los clips siguen

    // Los clips de B se renderizan en negro y en silencio: el grafo no lo abre.
    const engine::TimelineSnapshot snap{otro.project().timeline(), otro.project().media(), otro.project().canvas()};
    const auto g = engine::FilterGraphBuilder::build(snap, {320, 240, {25, 1}}, 0);
    for (const auto& in : g.inputs) CHECK(in.mediaPath != b);
}
