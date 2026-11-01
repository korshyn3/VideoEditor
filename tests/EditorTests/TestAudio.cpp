// TestAudio.cpp
// Escenarios de specs/audio-tracks (fase 5): separar el audio de un clip de video,
// clips de audio independientes, importación de audio y mezcla/mute en el grafo.
#include <doctest/doctest.h>

#include "EditorController/EditorController.h"
#include "EditorEngine/FilterGraphBuilder.h"
#include "EditorModel/Project.h"
#include "MediosPrueba.h"

#include <algorithm>

using namespace model;
using engine::FilterGraphBuilder;

namespace {

TimeUs s(double segundos) { return segundosATiempo(segundos); }

MediaInfo videoFalso(const std::string& nombre, double segundos, bool conAudio = true) {
    MediaInfo m;
    m.path = nombre;
    m.duration = segundosATiempo(segundos);
    m.hasVideo = true;
    m.width = 1280;
    m.height = 720;
    m.fps = {25, 1};
    m.hasAudio = conAudio;
    return m;
}

MediaInfo musicaFalsa(const std::string& nombre, double segundos) {
    MediaInfo m;
    m.path = nombre;
    m.duration = segundosATiempo(segundos);
    m.hasAudio = true;
    return m;
}

Id pista(const Project& p, TrackKind kind) { return p.timeline().firstTrack(kind)->id; }

engine::GraphDescription grafo(const Project& p) {
    return FilterGraphBuilder::build({p.timeline(), p.media(), p.canvas()}, {640, 360, {25, 1}}, 0);
}

int entradasDeAudio(const engine::GraphDescription& d) {
    return static_cast<int>(std::count_if(d.inputs.begin(), d.inputs.end(), [](const auto& in) { return !in.video; }));
}

bool contiene(const std::string& txt, const std::string& sub) { return txt.find(sub) != std::string::npos; }

std::string silencio(const std::string& segundos) {
    return "anullsrc=r=48000:cl=stereo,aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo,"
           "atrim=duration=" + segundos;
}

} // namespace

// ---------------------------------------------------------------------------
// 5.1 Separar audio
// ---------------------------------------------------------------------------

TEST_CASE("Separar audio") {
    Project p;
    const Id v1 = pista(p, TrackKind::Video);
    Id clip = kSinId;
    p.appendMedia(v1, p.addMedia(videoFalso("A.mp4", 7)), &clip);
    REQUIRE(p.moveClip(clip, v1, s(5)));  // el clip ocupa 5–12 s

    Id audio = kSinId;
    REQUIRE(p.separateAudio(clip, &audio));
    const Track* pistaAudio = nullptr;
    const Clip* a = p.timeline().findClip(audio, &pistaAudio);
    REQUIRE(a != nullptr);
    CHECK(pistaAudio->kind == TrackKind::Audio);
    CHECK(pistaAudio->name == "A1");
    CHECK(a->timelineStart == s(5));
    CHECK(a->timelineEnd() == s(12));
    CHECK(a->sourceIn == 0);
    CHECK(a->sourceOut == s(7));
    CHECK(a->mediaId == p.timeline().findClip(clip)->mediaId);
    CHECK_FALSE(p.timeline().findClip(clip)->linkedAudio);  // el video ya no aporta audio

    // En el grafo, el audio sale ahora de la pista A1 y no del clip de video.
    CHECK(entradasDeAudio(grafo(p)) == 1);
}

TEST_CASE("Separar audio sin pista de audio libre crea una nueva") {
    Project p;
    Id clip = kSinId;
    p.appendMedia(pista(p, TrackKind::Video), p.addMedia(videoFalso("A.mp4", 5)), &clip);
    p.appendMedia(pista(p, TrackKind::Audio), p.addMedia(musicaFalsa("musica.mp3", 60)));  // A1 ocupada 0–60

    Id audio = kSinId;
    REQUIRE(p.separateAudio(clip, &audio));
    const Track* pistaAudio = nullptr;
    REQUIRE(p.timeline().findClip(audio, &pistaAudio));
    CHECK(pistaAudio->name == "A2");
    CHECK(p.timeline().countTracks(TrackKind::Audio) == 2);
}

TEST_CASE("Clip sin audio") {
    Project p;
    Id clip = kSinId;
    p.appendMedia(pista(p, TrackKind::Video), p.addMedia(videoFalso("mudo.mp4", 5, false)), &clip);
    const auto rev = p.revision();
    const OpResult r = p.separateAudio(clip);
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.error.empty());
    CHECK(p.revision() == rev);
    CHECK(p.timeline().findTrack(pista(p, TrackKind::Audio))->clips.empty());
}

TEST_CASE("Separar dos veces o desde un clip de audio se rechaza") {
    Project p;
    Id clip = kSinId;
    p.appendMedia(pista(p, TrackKind::Video), p.addMedia(videoFalso("A.mp4", 5)), &clip);
    Id audio = kSinId;
    REQUIRE(p.separateAudio(clip, &audio));
    CHECK_FALSE(p.separateAudio(clip).ok);   // ya separado
    CHECK_FALSE(p.separateAudio(audio).ok);  // es un clip de audio
}

TEST_CASE("Controller: separar audio selecciona el clip de audio") {
    controller::EditorController ctrl;
    const auto r = ctrl.importFiles({medioPrueba("video_audio.mp4")});
    REQUIRE(r.errores.empty());
    ctrl.seleccionarClip(r.clips.at(0));
    REQUIRE(ctrl.separateAudio());
    const Track* pistaSel = ctrl.project().timeline().findTrack(ctrl.seleccion().trackId);
    REQUIRE(pistaSel != nullptr);
    CHECK(pistaSel->kind == TrackKind::Audio);
    CHECK(ctrl.seleccion().clipId != r.clips.at(0));
}

// ---------------------------------------------------------------------------
// 5.2 Clips de audio independientes y mezcla
// ---------------------------------------------------------------------------

TEST_CASE("Mover el audio separado") {
    Project p;
    Id clip = kSinId, audio = kSinId;
    p.appendMedia(pista(p, TrackKind::Video), p.addMedia(videoFalso("A.mp4", 5)), &clip);  // 0–5 s
    REQUIRE(p.separateAudio(clip, &audio));
    REQUIRE(p.moveClip(audio, pista(p, TrackKind::Audio), s(2)));  // audio 2–7 s

    // La imagen no cambia de lugar.
    CHECK(p.timeline().findClip(clip)->timelineStart == 0);
    const auto d = grafo(p);
    CHECK(d.fin == s(7));
    // El audio suena 2 s después: la pista A1 empieza con 2 s de silencio.
    CHECK(contiene(d.filtros, silencio("2.000000")));
    CHECK(entradasDeAudio(d) == 1);
}

TEST_CASE("Borrar el audio separado") {
    Project p;
    Id clip = kSinId, audio = kSinId;
    p.appendMedia(pista(p, TrackKind::Video), p.addMedia(videoFalso("A.mp4", 5)), &clip);
    REQUIRE(p.separateAudio(clip, &audio));
    REQUIRE(p.removeClip(audio, false));

    // Ese tramo del video queda en silencio: no hay entradas de audio, solo anullsrc.
    const auto d = grafo(p);
    CHECK(entradasDeAudio(d) == 0);
    CHECK(contiene(d.filtros, silencio("5.000000")));
    CHECK_FALSE(contiene(d.filtros, "amix"));
}

TEST_CASE("Música sobre video con audio") {
    Project p;
    p.appendMedia(pista(p, TrackKind::Video), p.addMedia(videoFalso("A.mp4", 5)));
    p.appendMedia(pista(p, TrackKind::Audio), p.addMedia(musicaFalsa("musica.mp3", 5)));

    const auto d = grafo(p);
    CHECK(entradasDeAudio(d) == 2);  // audio vinculado + música
    CHECK(contiene(d.filtros, "amix=inputs=2:duration=longest:normalize=0"));
}

TEST_CASE("Pista silenciada") {
    Project p;
    p.appendMedia(pista(p, TrackKind::Video), p.addMedia(videoFalso("A.mp4", 5)));
    Id a2 = kSinId;
    p.addTrack(TrackKind::Audio, &a2);
    p.appendMedia(a2, p.addMedia(musicaFalsa("musica.mp3", 5)));

    REQUIRE(p.setTrackMuted(a2, true));
    CHECK(p.timeline().findTrack(a2)->muted);
    auto d = grafo(p);
    CHECK(entradasDeAudio(d) == 1);  // solo el audio vinculado del video
    CHECK_FALSE(contiene(d.filtros, "amix"));
    for (const auto& in : d.inputs) CHECK(in.mediaPath != "musica.mp3");

    REQUIRE(p.setTrackMuted(a2, false));
    CHECK(entradasDeAudio(grafo(p)) == 2);

    // Las pistas de video no se silencian.
    CHECK_FALSE(p.setTrackMuted(pista(p, TrackKind::Video), true).ok);
}

TEST_CASE("Se mezcla el audio de un clip en una pista de video superior") {
    Project p;
    Id v2 = kSinId;
    p.addTrack(TrackKind::Video, &v2);
    p.appendMedia(v2, p.addMedia(videoFalso("pip.mp4", 3)));
    p.appendMedia(pista(p, TrackKind::Video), p.addMedia(videoFalso("A.mp4", 5, false)));
    CHECK(entradasDeAudio(grafo(p)) == 1);
}

// ---------------------------------------------------------------------------
// 5.3 Importación de audio
// ---------------------------------------------------------------------------

TEST_CASE("Importar música") {
    controller::EditorController ctrl;
    REQUIRE(ctrl.addTrack(TrackKind::Audio));  // crea A2 vacía y la selecciona
    const Id a2 = ctrl.seleccion().trackId;
    const auto r = ctrl.importFiles({medioPrueba("tono.mp3"), medioPrueba("tono.wav"), medioPrueba("tono.aac")});
    REQUIRE(r.errores.empty());
    const Track* t = ctrl.project().timeline().findTrack(a2);
    REQUIRE(t->clips.size() == 3);
    CHECK(t->clips[0].timelineStart == 0);
    CHECK(t->clips[0].duration() == ctrl.project().media().find(t->clips[0].mediaId)->duration);
    CHECK(t->clips[1].timelineStart == t->clips[0].timelineEnd());  // en orden, sin hueco
    CHECK(t->clips[2].timelineStart == t->clips[1].timelineEnd());
    CHECK(ctrl.project().timeline().firstTrack(TrackKind::Audio)->clips.empty());  // A1 no se tocó
}

TEST_CASE("Importar video en pista de audio") {
    controller::EditorController ctrl;
    const Id a1 = ctrl.project().timeline().firstTrack(TrackKind::Audio)->id;
    ctrl.seleccionarPista(a1);

    // Un video con audio: se coloca solo su audio en A1.
    auto r = ctrl.importFiles({medioPrueba("video_audio.mp4")});
    REQUIRE(r.errores.empty());
    const Track* pistaClip = nullptr;
    REQUIRE(ctrl.project().timeline().findClip(r.clips.at(0), &pistaClip));
    CHECK(pistaClip->id == a1);
    CHECK(ctrl.project().timeline().firstTrack(TrackKind::Video)->clips.empty());

    // Un video sin audio: se rechaza con un mensaje.
    r = ctrl.importFiles({medioPrueba("solo_video.mp4")});
    CHECK(r.clips.empty());
    REQUIRE(r.errores.size() == 1);
    CHECK(contiene(r.errores[0], "solo_video.mp4"));
    CHECK(ctrl.project().timeline().findTrack(a1)->clips.size() == 1);
}
