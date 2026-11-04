// TestSeleccion.cpp
// Capa: TESTS.
// Escenarios de specs/timeline-editing (tarea 5.8): selección múltiple, dividir y
// eliminar sobre toda la selección, mover varios clips juntos y mover sobrescribiendo.
#include <doctest/doctest.h>

#include "EditorController/EditorController.h"
#include "EditorModel/Project.h"
#include "MediosPrueba.h"

using namespace model;

namespace {

TimeUs s(double segundos) { return segundosATiempo(segundos); }

MediaInfo videoFalso(const std::string& nombre, double segundos, bool conAudio = false) {
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

MediaInfo musicaFalsa(double segundos) {
    MediaInfo m;
    m.path = "musica.mp3";
    m.duration = segundosATiempo(segundos);
    m.hasAudio = true;
    return m;
}

Id v1De(const Project& p) { return p.timeline().firstTrack(TrackKind::Video)->id; }
Id a1De(const Project& p) { return p.timeline().firstGeneralAudioTrack()->id; }
const Clip& clipDe(const Project& p, Id id) { return *p.timeline().findClip(id); }
const Track& pistaV1(const Project& p) { return *p.timeline().findTrack(v1De(p)); }

// Clip de video en V1 en la posición indicada.
Id colocar(Project& p, const MediaInfo& m, double inicio) {
    Id id = kSinId;
    p.appendMedia(v1De(p), p.addMedia(m), &id);
    p.moveClip(id, v1De(p), s(inicio));
    return id;
}

} // namespace

// ---------------------------------------------------------------------------
// Selección y acciones sobre la selección (controller)
// ---------------------------------------------------------------------------

TEST_CASE("Seleccionar video y música") {
    controller::EditorController ctrl;
    const auto r = ctrl.importFiles({medioPrueba("video_audio.mp4"), medioPrueba("tono.mp3")});
    REQUIRE(r.errores.empty());
    const Id video = r.clips.at(0), musica = r.clips.at(1);

    ctrl.seleccionarClip(video);
    ctrl.alternarSeleccionClip(musica);  // Ctrl+clic
    CHECK(ctrl.seleccion().clips.size() == 2);
    CHECK(ctrl.seleccion().contiene(video));
    CHECK(ctrl.seleccion().contiene(musica));
    CHECK(ctrl.seleccion().clipId == musica);  // el último es el principal

    ctrl.alternarSeleccionClip(musica);  // Ctrl+clic otra vez: se quita
    CHECK(ctrl.seleccion().clips.size() == 1);
    CHECK(ctrl.seleccion().clipId == video);

    ctrl.alternarSeleccionClip(musica);
    ctrl.seleccionarClip(musica);  // clic normal: queda solo ese
    CHECK(ctrl.seleccion().clips.size() == 1);
    CHECK(ctrl.seleccion().clipId == musica);
}

TEST_CASE("Dividir la selección") {
    controller::EditorController ctrl;
    const auto r = ctrl.importFiles({medioPrueba("video_audio.mp4"), medioPrueba("tono.mp3")});  // 5 s y 3 s
    REQUIRE(r.errores.empty());
    const Id video = r.clips.at(0), musica = r.clips.at(1);
    const Id audioVideo = ctrl.project().timeline().findClip(video)->linkedClipId;
    ctrl.seleccionarClip(video);
    ctrl.alternarSeleccionClip(audioVideo);  // el audio vinculado también seleccionado
    ctrl.alternarSeleccionClip(musica);

    ctrl.setPlayhead(s(4));  // dentro del video, fuera de la música
    REQUIRE(ctrl.splitAtPlayhead());
    const model::Timeline& tl = ctrl.project().timeline();
    CHECK(tl.firstTrack(TrackKind::Video)->clips.size() == 2);
    CHECK(tl.audioTrackOf(tl.firstTrack(TrackKind::Video)->id)->clips.size() == 2);  // dividido una sola vez
    CHECK(tl.firstGeneralAudioTrack()->clips.size() == 1);                            // la música no cambia
    CHECK(tl.findClip(video)->timelineEnd() == s(4));  // la selección queda en la parte izquierda

    // Si ningún clip seleccionado está bajo el cursor, no cambia nada.
    ctrl.seleccionarClip(musica);
    const auto rev = ctrl.project().revision();
    CHECK_FALSE(ctrl.splitAtPlayhead().ok);
    CHECK(ctrl.project().revision() == rev);
}

TEST_CASE("Eliminar la selección") {
    controller::EditorController ctrl;
    REQUIRE(ctrl.importFiles({medioPrueba("video_audio.mp4"), medioPrueba("tono.mp3")}).errores.empty());
    REQUIRE(ctrl.addTrack(TrackKind::Video));  // V2, seleccionada
    const auto r2 = ctrl.importFiles({medioPrueba("solo_video.mp4")});
    REQUIRE(r2.errores.empty());

    // Un clip de cada pista: video de V1, música de A1 y video de V2.
    const model::Timeline& tl = ctrl.project().timeline();
    ctrl.seleccionarClip(tl.firstTrack(TrackKind::Video)->clips.at(0).id);
    ctrl.alternarSeleccionClip(tl.firstGeneralAudioTrack()->clips.at(0).id);
    ctrl.alternarSeleccionClip(r2.clips.at(0));
    REQUIRE(ctrl.seleccion().clips.size() == 3);

    REQUIRE(ctrl.deleteSelectedClip(false));
    CHECK(ctrl.project().timeline().empty());  // también el audio vinculado del video
    CHECK(ctrl.seleccion().clips.empty());
    CHECK(ctrl.seleccion().clipId == kSinId);
}

TEST_CASE("Eliminar la selección con ripple") {
    controller::EditorController ctrl;
    // Tres videos sin audio seguidos: 0–3, 3–6, 6–9.
    const auto r = ctrl.importFiles(
        {medioPrueba("solo_video.mp4"), medioPrueba("solo_video.mp4"), medioPrueba("solo_video.mp4")});
    REQUIRE(r.errores.empty());
    ctrl.seleccionarClip(r.clips[0]);
    ctrl.alternarSeleccionClip(r.clips[1]);
    REQUIRE(ctrl.deleteSelectedClip(true));
    const Track* v1 = ctrl.project().timeline().firstTrack(TrackKind::Video);
    REQUIRE(v1->clips.size() == 1);
    CHECK(v1->clips[0].id == r.clips[2]);
    CHECK(v1->clips[0].timelineStart == 0);  // se cerraron los dos huecos
}

TEST_CASE("Recortar con varios clips seleccionados") {
    controller::EditorController ctrl;
    const auto r = ctrl.importFiles({medioPrueba("video_audio.mp4"), medioPrueba("tono.mp3")});
    REQUIRE(r.errores.empty());
    ctrl.seleccionarClip(r.clips.at(0));
    ctrl.alternarSeleccionClip(r.clips.at(1));
    CHECK(ctrl.seleccion().multiple());
    // Desvincular no está disponible con varios (la vista también desactiva el recorte).
    CHECK_FALSE(ctrl.unlinkAudio().ok);
    CHECK(ctrl.project().timeline().findClip(r.clips.at(0))->vinculado());
}

// ---------------------------------------------------------------------------
// Mover varios clips juntos (modelo)
// ---------------------------------------------------------------------------

TEST_CASE("Mover la selección") {
    Project p;
    Id video = kSinId, musica = kSinId;
    p.appendMedia(v1De(p), p.addMedia(videoFalso("A.mp4", 5, true)), &video);
    p.appendMedia(a1De(p), p.addMedia(musicaFalsa(4)), &musica);
    const Id audio = clipDe(p, video).linkedClipId;

    REQUIRE(p.moveClips({video, musica}, s(3)));
    CHECK(clipDe(p, video).timelineStart == s(3));
    CHECK(clipDe(p, audio).timelineStart == s(3));   // el audio vinculado también
    CHECK(clipDe(p, musica).timelineStart == s(3));
    // Cada uno sigue en su pista.
    CHECK(pistaV1(p).findClip(video) != nullptr);
    CHECK(p.timeline().findTrack(a1De(p))->findClip(musica) != nullptr);

    // Hacia la izquierda, el grupo no puede pasar de 0.
    REQUIRE(p.moveClips({video, musica}, s(-10)));
    CHECK(clipDe(p, video).timelineStart == 0);
}

TEST_CASE("Movimiento de la selección con solapamiento") {
    Project p;
    const Id a = colocar(p, videoFalso("A.mp4", 5), 0);  // 0–5
    const Id b = colocar(p, videoFalso("B.mp4", 5), 5);  // 5–10
    Id musica = kSinId;
    p.appendMedia(a1De(p), p.addMedia(musicaFalsa(3)), &musica);

    const auto rev = p.revision();
    CHECK_FALSE(p.moveClips({a, musica}, s(2)).ok);  // A quedaría encima de B
    CHECK(p.revision() == rev);
    CHECK(clipDe(p, a).timelineStart == 0);
    CHECK(clipDe(p, b).timelineStart == s(5));
    CHECK(clipDe(p, musica).timelineStart == 0);
}

// ---------------------------------------------------------------------------
// Mover sobrescribiendo (Shift)
// ---------------------------------------------------------------------------

TEST_CASE("Sobrescribir el final de un clip") {
    Project p;
    const Id a = colocar(p, videoFalso("A.mp4", 10), 0);   // 0–10
    const Id b = colocar(p, videoFalso("B.mp4", 5), 10);   // 10–15
    CHECK_FALSE(p.moveClip(b, v1De(p), s(8)).ok);          // sin Shift: se rechaza
    REQUIRE(p.moveClip(b, v1De(p), s(8), true));           // con Shift
    CHECK(clipDe(p, b).timelineStart == s(8));
    CHECK(clipDe(p, b).timelineEnd() == s(13));
    CHECK(clipDe(p, a).timelineStart == 0);
    CHECK(clipDe(p, a).timelineEnd() == s(8));
    CHECK(clipDe(p, a).sourceOut == s(8));
}

TEST_CASE("Sobrescribir el inicio de un clip") {
    Project p;
    const Id a = colocar(p, videoFalso("A.mp4", 10), 5);  // 5–15
    const Id b = colocar(p, videoFalso("B.mp4", 4), 0);   // 0–4
    REQUIRE(p.moveClip(b, v1De(p), s(3), true));          // B: 3–7, tapa 5–7 de A
    CHECK(clipDe(p, a).timelineStart == s(7));
    CHECK(clipDe(p, a).sourceIn == s(2));
    CHECK(clipDe(p, a).timelineEnd() == s(15));
}

TEST_CASE("Sobrescribir el medio de un clip") {
    Project p;
    const Id a = colocar(p, videoFalso("A.mp4", 10), 0);  // 0–10
    const Id b = colocar(p, videoFalso("B.mp4", 2), 10);  // 10–12
    REQUIRE(p.moveClip(b, v1De(p), s(4), true));

    const Track& v1 = pistaV1(p);
    REQUIRE(v1.clips.size() == 3);
    CHECK(v1.clips[0].id == a);
    CHECK(v1.clips[0].timelineEnd() == s(4));
    CHECK(v1.clips[0].sourceOut == s(4));
    CHECK(v1.clips[1].id == b);
    CHECK(v1.clips[1].timelineStart == s(4));
    CHECK(v1.clips[2].timelineStart == s(6));
    CHECK(v1.clips[2].sourceIn == s(6));
    CHECK(v1.clips[2].sourceOut == s(10));
    CHECK(v1.clips[2].mediaId == v1.clips[0].mediaId);
}

TEST_CASE("Tapar un clip completo") {
    Project p;
    const Id a = colocar(p, videoFalso("A.mp4", 2), 0);  // 0–2
    const Id b = colocar(p, videoFalso("B.mp4", 5), 2);  // 2–7
    REQUIRE(p.moveClip(b, v1De(p), 0, true));            // B: 0–5 tapa todo A
    CHECK(p.timeline().findClip(a) == nullptr);
    REQUIRE(pistaV1(p).clips.size() == 1);
    CHECK(clipDe(p, b).timelineStart == 0);
}

TEST_CASE("Sobrescribir un video con audio vinculado") {
    Project p;
    const Id a = colocar(p, videoFalso("A.mp4", 10, true), 0);  // 0–10 + audio en Audio V1
    const Id m = colocar(p, videoFalso("M.mp4", 2), 10);        // 10–12, sin audio
    REQUIRE(p.moveClip(m, v1De(p), s(4), true));

    const Track& audioV1 = *p.timeline().audioTrackOf(v1De(p));
    const Track& v1 = pistaV1(p);
    REQUIRE(v1.clips.size() == 3);
    REQUIRE(audioV1.clips.size() == 2);  // el audio también perdió el tramo 4–6
    CHECK(audioV1.clips[0].timelineEnd() == s(4));
    CHECK(audioV1.clips[1].timelineStart == s(6));
    CHECK(audioV1.clips[1].sourceIn == s(6));
    // Cada mitad de video sigue vinculada a su mitad de audio.
    CHECK(v1.clips[0].id == a);
    CHECK(v1.clips[0].linkedClipId == audioV1.clips[0].id);
    CHECK(audioV1.clips[1].linkedClipId == v1.clips[2].id);
    CHECK(v1.clips[2].linkedClipId == audioV1.clips[1].id);
}

TEST_CASE("Sobrescribir con un grupo") {
    Project p;
    const Id a = colocar(p, videoFalso("A.mp4", 10), 0);  // V1 0–10
    Id musicaBase = kSinId, musica = kSinId, a2 = kSinId;
    p.appendMedia(a1De(p), p.addMedia(musicaFalsa(10)), &musicaBase);  // A1 0–10
    p.addTrack(TrackKind::Audio, &a2);
    p.appendMedia(a2, p.addMedia(musicaFalsa(2)), &musica);
    REQUIRE(p.moveClip(musica, a2, s(8)));                // A2 8–10
    const Id b = colocar(p, videoFalso("B.mp4", 2), 12);  // V1 12–14

    // B y la música de A2 se mueven 8 s hacia atrás con Shift: B cae en 4–6 sobre A y la
    // música queda en 0–2 de A2 (donde no tapa nada).
    REQUIRE(p.moveClips({b, musica}, s(-8), true));
    CHECK(clipDe(p, b).timelineStart == s(4));
    CHECK(clipDe(p, musica).timelineStart == 0);
    CHECK(pistaV1(p).clips.size() == 3);              // A quedó dividido alrededor de B
    CHECK(clipDe(p, a).timelineEnd() == s(4));
    CHECK(clipDe(p, musicaBase).timelineStart == 0);  // A1 no se tocó
    CHECK(clipDe(p, musicaBase).timelineEnd() == s(10));
}

TEST_CASE("Los restos de menos de un cuadro se descartan") {
    Project p;  // lienzo a 25 fps: 1 cuadro = 40 ms
    const Id a = colocar(p, videoFalso("A.mp4", 10), 0);
    const Id b = colocar(p, videoFalso("B.mp4", 2), 10);
    REQUIRE(p.moveClip(b, v1De(p), s(0.02), true));  // dejaría 20 ms de A al inicio
    const Track& v1 = pistaV1(p);
    REQUIRE(v1.clips.size() == 2);
    CHECK(v1.clips[0].id == b);
    CHECK(v1.clips[1].timelineStart == s(2.02));
    CHECK(p.timeline().findClip(a) == nullptr);  // el trozo izquierdo (el original) se fue
}
