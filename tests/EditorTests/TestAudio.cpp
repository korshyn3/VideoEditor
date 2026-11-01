// TestAudio.cpp
// Escenarios de specs/audio-tracks (fase 5): pistas de audio de video y generales, audio
// vinculado a los clips de video, operaciones sobre clips vinculados, desvincular,
// importación de audio y mezcla/mute en el grafo.
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

Id v1De(const Project& p) { return p.timeline().firstTrack(TrackKind::Video)->id; }
Id audioDe(const Project& p, Id pistaVideo) { return p.timeline().audioTrackOf(pistaVideo)->id; }
Id a1De(const Project& p) { return p.timeline().firstGeneralAudioTrack()->id; }

// Pista en la que está un clip.
Id pistaDe(const Project& p, Id clip) {
    const Track* t = nullptr;
    p.timeline().findClip(clip, &t);
    return t ? t->id : kSinId;
}

const Clip& clipDe(const Project& p, Id id) { return *p.timeline().findClip(id); }

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

// Proyecto con un video con audio A (`segundos`) en V1; devuelve los ids de los clips.
struct ConVideo {
    Project p;
    Id video = kSinId, audio = kSinId;
    explicit ConVideo(double segundos = 5) {
        p.appendMedia(v1De(p), p.addMedia(videoFalso("A.mp4", segundos)), &video);
        audio = clipDe(p, video).linkedClipId;
    }
};

} // namespace

// ---------------------------------------------------------------------------
// 5.5 Tipos de pista de audio y audio vinculado
// ---------------------------------------------------------------------------

TEST_CASE("Agregar una pista de video trae su pista de audio") {
    Project p;
    Id v2 = kSinId;
    REQUIRE(p.addTrack(TrackKind::Video, &v2));
    const Track* audioV2 = p.timeline().audioTrackOf(v2);
    REQUIRE(audioV2 != nullptr);
    CHECK(audioV2->name == "Audio V2");
    CHECK(audioV2->clips.empty());

    // Orden: V1, V2, Audio V1, Audio V2, A1 (la vista dibuja V2 arriba de V1).
    const auto& t = p.timeline().tracks;
    REQUIRE(t.size() == 5);
    CHECK(t[0].name == "V1");
    CHECK(t[1].name == "V2");
    CHECK(t[2].name == "Audio V1");
    CHECK(t[3].name == "Audio V2");
    CHECK(t[4].name == "A1");
}

TEST_CASE("Agregar una pista de audio") {
    Project p;
    Id a2 = kSinId;
    REQUIRE(p.addTrack(TrackKind::Audio, &a2));
    const Track* t = p.timeline().findTrack(a2);
    CHECK(t->esAudioGeneral());
    CHECK(t->name == "A2");
    CHECK(p.timeline().tracks.back().id == a2);
}

TEST_CASE("Eliminar una pista de audio de video") {
    Project p;
    const Id audioV1 = audioDe(p, v1De(p));
    const auto rev = p.revision();
    const OpResult r = p.removeTrack(audioV1);
    CHECK_FALSE(r.ok);
    CHECK(contiene(r.error, "V1"));
    CHECK(p.revision() == rev);
    CHECK(p.timeline().findTrack(audioV1) != nullptr);
}

TEST_CASE("Eliminar una pista de video elimina su pista de audio y desvincula") {
    Project p;
    Id v2 = kSinId;
    p.addTrack(TrackKind::Video, &v2);
    const Id audioV2 = audioDe(p, v2);
    Id video = kSinId;
    REQUIRE(p.appendMedia(v2, p.addMedia(videoFalso("A.mp4", 5)), &video));
    REQUIRE(p.removeTrack(v2));
    CHECK(p.timeline().findTrack(audioV2) == nullptr);
    CHECK(p.timeline().empty());
}

TEST_CASE("Importar un video con audio") {
    Project p;
    Id video = kSinId;
    REQUIRE(p.appendMedia(v1De(p), p.addMedia(videoFalso("A.mp4", 10)), &video));
    const Clip& v = clipDe(p, video);
    REQUIRE(v.vinculado());
    const Clip& a = clipDe(p, v.linkedClipId);
    CHECK(pistaDe(p, a.id) == audioDe(p, v1De(p)));
    CHECK(a.linkedClipId == video);
    CHECK(a.mediaId == v.mediaId);
    CHECK(a.timelineStart == 0);
    CHECK(a.timelineEnd() == s(10));
    CHECK(a.sourceIn == v.sourceIn);
    CHECK(a.sourceOut == v.sourceOut);
    CHECK(p.timeline().findTrack(a1De(p))->clips.empty());  // la pista general no se toca
}

TEST_CASE("Importar un video sin audio") {
    Project p;
    Id video = kSinId;
    REQUIRE(p.appendMedia(v1De(p), p.addMedia(videoFalso("mudo.mp4", 5, false)), &video));
    CHECK_FALSE(clipDe(p, video).vinculado());
    CHECK(p.timeline().findTrack(audioDe(p, v1De(p)))->clips.empty());
}

TEST_CASE("Desvincular audio") {
    ConVideo c(7);
    REQUIRE(c.p.moveClip(c.video, v1De(c.p), s(5)));  // 5–12 s
    REQUIRE(c.p.unlinkAudio(c.video));
    CHECK_FALSE(clipDe(c.p, c.video).vinculado());
    CHECK_FALSE(clipDe(c.p, c.audio).vinculado());
    // El audio sigue en 5–12 s en su pista...
    CHECK(clipDe(c.p, c.audio).timelineStart == s(5));
    CHECK(pistaDe(c.p, c.audio) == audioDe(c.p, v1De(c.p)));
    // ...y ya es independiente: moverlo no mueve el video.
    REQUIRE(c.p.moveClip(c.audio, pistaDe(c.p, c.audio), s(8)));
    CHECK(clipDe(c.p, c.video).timelineStart == s(5));
}

TEST_CASE("Desvincular desde el clip de audio") {
    ConVideo c;
    REQUIRE(c.p.unlinkAudio(c.audio));
    CHECK_FALSE(clipDe(c.p, c.video).vinculado());
}

TEST_CASE("Clip sin audio") {
    Project p;
    Id video = kSinId;
    p.appendMedia(v1De(p), p.addMedia(videoFalso("mudo.mp4", 5, false)), &video);
    const auto rev = p.revision();
    const OpResult r = p.unlinkAudio(video);
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.error.empty());
    CHECK(p.revision() == rev);

    ConVideo c;
    REQUIRE(c.p.unlinkAudio(c.video));
    CHECK_FALSE(c.p.unlinkAudio(c.video).ok);  // ya desvinculado
}

// ---------------------------------------------------------------------------
// 5.6 Operaciones sobre clips vinculados
// ---------------------------------------------------------------------------

TEST_CASE("Mover el video a otra pista") {
    ConVideo c;
    Id v2 = kSinId;
    c.p.addTrack(TrackKind::Video, &v2);
    REQUIRE(c.p.moveClip(c.video, v2, s(3)));
    CHECK(pistaDe(c.p, c.video) == v2);
    CHECK(pistaDe(c.p, c.audio) == audioDe(c.p, v2));  // de Audio V1 a Audio V2
    CHECK(clipDe(c.p, c.audio).timelineStart == s(3));
    CHECK(clipDe(c.p, c.video).timelineStart == s(3));
}

TEST_CASE("Mover el audio vinculado") {
    ConVideo c;
    Id a2 = kSinId;
    c.p.addTrack(TrackKind::Audio, &a2);
    // Se pide moverlo a A2, pero un audio vinculado solo se mueve en el tiempo.
    REQUIRE(c.p.moveClip(c.audio, a2, s(2)));
    CHECK(clipDe(c.p, c.video).timelineStart == s(2));
    CHECK(clipDe(c.p, c.audio).timelineStart == s(2));
    CHECK(pistaDe(c.p, c.audio) == audioDe(c.p, v1De(c.p)));
}

TEST_CASE("Recortar un clip vinculado") {
    ConVideo c(10);
    REQUIRE(c.p.trimClip(c.audio, s(2), s(10)));  // 2 s menos al inicio, desde el audio
    for (Id id : {c.video, c.audio}) {
        CAPTURE(id);
        CHECK(clipDe(c.p, id).sourceIn == s(2));
        CHECK(clipDe(c.p, id).timelineStart == s(2));
        CHECK(clipDe(c.p, id).timelineEnd() == s(10));
    }
}

TEST_CASE("Recorte rechazado por el vecino del audio") {
    ConVideo c(10);
    REQUIRE(c.p.trimClip(c.video, 0, s(4)));  // A: 0–4 (video y audio)
    // D se coloca a continuación (4–14) y se desvincula; su video pasa a V2, así que en
    // Audio V1 queda un audio suelto en 4–14 y V1 queda libre después de A.
    Id d = kSinId, v2 = kSinId;
    REQUIRE(c.p.appendMedia(v1De(c.p), c.p.addMedia(videoFalso("D.mp4", 10)), &d));
    REQUIRE(c.p.unlinkAudio(d));
    c.p.addTrack(TrackKind::Video, &v2);
    REQUIRE(c.p.moveClip(d, v2, s(4)));

    const auto rev = c.p.revision();
    // Extender A hasta 10 s está libre en V1, pero su audio chocaría con el de D.
    CHECK_FALSE(c.p.trimClip(c.video, 0, s(10)).ok);
    CHECK(c.p.revision() == rev);
    CHECK(clipDe(c.p, c.audio).timelineEnd() == s(4));
}

TEST_CASE("Eliminar un clip vinculado") {
    Project p;
    Id a = kSinId, b = kSinId, c = kSinId;
    p.appendMedia(v1De(p), p.addMedia(videoFalso("A.mp4", 10)), &a);
    p.appendMedia(v1De(p), p.addMedia(videoFalso("B.mp4", 5)), &b);
    p.appendMedia(v1De(p), p.addMedia(videoFalso("C.mp4", 8)), &c);
    const Id audioB = clipDe(p, b).linkedClipId;
    const Id audioC = clipDe(p, c).linkedClipId;

    REQUIRE(p.removeClip(b, true));
    CHECK(p.timeline().findClip(audioB) == nullptr);
    CHECK(clipDe(p, c).timelineStart == s(10));       // hueco cerrado en V1...
    CHECK(clipDe(p, audioC).timelineStart == s(10));  // ...y en Audio V1
}

TEST_CASE("Eliminar desde el audio vinculado sin ripple") {
    ConVideo c;
    REQUIRE(c.p.removeClip(c.audio, false));
    CHECK(c.p.timeline().findClip(c.video) == nullptr);
    CHECK(c.p.timeline().empty());
}

TEST_CASE("El ripple corre también los audios vinculados de los clips siguientes") {
    Project p;
    Id mudo = kSinId, b = kSinId;
    p.appendMedia(v1De(p), p.addMedia(videoFalso("mudo.mp4", 4, false)), &mudo);  // 0–4, sin audio
    p.appendMedia(v1De(p), p.addMedia(videoFalso("B.mp4", 5)), &b);                // 4–9 + audio
    REQUIRE(p.removeClip(mudo, true));
    CHECK(clipDe(p, b).timelineStart == 0);
    CHECK(clipDe(p, clipDe(p, b).linkedClipId).timelineStart == 0);
}

TEST_CASE("Movimiento rechazado por el audio") {
    ConVideo c;  // A: 0–5 con audio en Audio V1
    REQUIRE(c.p.unlinkAudio(c.video));
    REQUIRE(c.p.moveClip(c.audio, audioDe(c.p, v1De(c.p)), s(10)));  // audio suelto en 10–15

    Id b = kSinId;
    REQUIRE(c.p.appendMedia(v1De(c.p), c.p.addMedia(videoFalso("B.mp4", 5)), &b));  // 15–20
    const auto rev = c.p.revision();
    // En V1 el hueco 6–11 está libre, pero en Audio V1 está el audio suelto (10–15).
    CHECK_FALSE(c.p.moveClip(b, v1De(c.p), s(6)).ok);
    CHECK(c.p.revision() == rev);
    CHECK(clipDe(c.p, b).timelineStart == s(15));
    CHECK(clipDe(c.p, clipDe(c.p, b).linkedClipId).timelineStart == s(15));
}

TEST_CASE("Mover un video desvinculado") {
    ConVideo c;
    Id v2 = kSinId;
    c.p.addTrack(TrackKind::Video, &v2);
    REQUIRE(c.p.unlinkAudio(c.video));
    REQUIRE(c.p.moveClip(c.video, v2, s(1)));
    CHECK(pistaDe(c.p, c.audio) == audioDe(c.p, v1De(c.p)));
    CHECK(clipDe(c.p, c.audio).timelineStart == 0);
}

TEST_CASE("Llevar el audio a una pista general") {
    ConVideo c;
    REQUIRE(c.p.unlinkAudio(c.video));
    REQUIRE(c.p.moveClip(c.audio, a1De(c.p), s(3)));
    CHECK(pistaDe(c.p, c.audio) == a1De(c.p));
    CHECK(clipDe(c.p, c.audio).timelineStart == s(3));
}

TEST_CASE("Controller: desvincular el clip seleccionado") {
    controller::EditorController ctrl;
    const auto r = ctrl.importFiles({medioPrueba("video_audio.mp4")});
    REQUIRE(r.errores.empty());
    CHECK_FALSE(ctrl.unlinkAudio().ok);  // nada seleccionado
    ctrl.seleccionarClip(r.clips.at(0));
    REQUIRE(ctrl.unlinkAudio());
    CHECK_FALSE(ctrl.project().timeline().findClip(r.clips.at(0))->vinculado());
    CHECK(ctrl.seleccion().clipId == r.clips.at(0));
}

// ---------------------------------------------------------------------------
// 5.2 Mezcla en el grafo
// ---------------------------------------------------------------------------

TEST_CASE("El audio sale de la pista de audio, no del clip de video") {
    ConVideo c;
    auto d = grafo(c.p);
    CHECK(entradasDeAudio(d) == 1);
    // Silenciar Audio V1 silencia el audio del video.
    REQUIRE(c.p.setTrackMuted(audioDe(c.p, v1De(c.p)), true));
    d = grafo(c.p);
    CHECK(entradasDeAudio(d) == 0);
}

TEST_CASE("Mover el audio desvinculado") {
    ConVideo c;  // 0–5 s
    REQUIRE(c.p.unlinkAudio(c.video));
    REQUIRE(c.p.moveClip(c.audio, pistaDe(c.p, c.audio), s(2)));  // audio 2–7 s

    // La imagen no cambia de lugar.
    CHECK(clipDe(c.p, c.video).timelineStart == 0);
    const auto d = grafo(c.p);
    CHECK(d.fin == s(7));
    // El audio suena 2 s después: su pista empieza con 2 s de silencio.
    CHECK(contiene(d.filtros, silencio("2.000000")));
    CHECK(entradasDeAudio(d) == 1);
}

TEST_CASE("Borrar el audio desvinculado") {
    ConVideo c;
    REQUIRE(c.p.unlinkAudio(c.video));
    REQUIRE(c.p.removeClip(c.audio, false));
    CHECK(c.p.timeline().findClip(c.video) != nullptr);

    // Ese tramo del video queda en silencio: no hay entradas de audio, solo anullsrc.
    const auto d = grafo(c.p);
    CHECK(entradasDeAudio(d) == 0);
    CHECK(contiene(d.filtros, silencio("5.000000")));
    CHECK_FALSE(contiene(d.filtros, "amix"));
}

TEST_CASE("Música sobre video con audio") {
    ConVideo c;
    c.p.appendMedia(a1De(c.p), c.p.addMedia(musicaFalsa("musica.mp3", 5)));

    const auto d = grafo(c.p);
    CHECK(entradasDeAudio(d) == 2);  // audio del video + música
    CHECK(contiene(d.filtros, "amix=inputs=2:duration=longest:normalize=0"));
}

TEST_CASE("Pista silenciada") {
    ConVideo c;
    Id a2 = kSinId;
    c.p.addTrack(TrackKind::Audio, &a2);
    c.p.appendMedia(a2, c.p.addMedia(musicaFalsa("musica.mp3", 5)));

    REQUIRE(c.p.setTrackMuted(a2, true));
    CHECK(c.p.timeline().findTrack(a2)->muted);
    auto d = grafo(c.p);
    CHECK(entradasDeAudio(d) == 1);  // solo el audio del video
    CHECK_FALSE(contiene(d.filtros, "amix"));
    for (const auto& in : d.inputs) CHECK(in.mediaPath != "musica.mp3");

    REQUIRE(c.p.setTrackMuted(a2, false));
    CHECK(entradasDeAudio(grafo(c.p)) == 2);

    // Las pistas de video no se silencian.
    CHECK_FALSE(c.p.setTrackMuted(v1De(c.p), true).ok);
}

// ---------------------------------------------------------------------------
// 5.7 Importación en el controller
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
    CHECK(ctrl.project().timeline().firstGeneralAudioTrack()->clips.empty());  // A1 no se tocó
}

TEST_CASE("La música nunca va a una pista de audio de video") {
    controller::EditorController ctrl;
    const model::Timeline& tl = ctrl.project().timeline();
    ctrl.seleccionarPista(tl.audioTrackOf(tl.firstTrack(TrackKind::Video)->id)->id);
    const auto r = ctrl.importFiles({medioPrueba("tono.mp3")});
    REQUIRE(r.errores.empty());
    CHECK(pistaDe(ctrl.project(), r.clips.at(0)) == tl.firstGeneralAudioTrack()->id);
}

TEST_CASE("Con Audio V2 seleccionada, un video va a V2") {
    controller::EditorController ctrl;
    REQUIRE(ctrl.addTrack(TrackKind::Video));
    const Id v2 = ctrl.seleccion().trackId;
    ctrl.seleccionarPista(ctrl.project().timeline().audioTrackOf(v2)->id);
    const auto r = ctrl.importFiles({medioPrueba("video_audio.mp4")});
    REQUIRE(r.errores.empty());
    CHECK(pistaDe(ctrl.project(), r.clips.at(0)) == v2);
}

TEST_CASE("Importar video en pista de audio") {
    controller::EditorController ctrl;
    const Id a1 = ctrl.project().timeline().firstGeneralAudioTrack()->id;
    ctrl.seleccionarPista(a1);

    // Un video con audio: se coloca solo su audio en A1, sin vínculo.
    auto r = ctrl.importFiles({medioPrueba("video_audio.mp4")});
    REQUIRE(r.errores.empty());
    CHECK(pistaDe(ctrl.project(), r.clips.at(0)) == a1);
    CHECK_FALSE(ctrl.project().timeline().findClip(r.clips.at(0))->vinculado());
    CHECK(ctrl.project().timeline().firstTrack(TrackKind::Video)->clips.empty());

    // Un video sin audio: se rechaza con un mensaje.
    r = ctrl.importFiles({medioPrueba("solo_video.mp4")});
    CHECK(r.clips.empty());
    REQUIRE(r.errores.size() == 1);
    CHECK(contiene(r.errores[0], "solo_video.mp4"));
    CHECK(ctrl.project().timeline().findTrack(a1)->clips.size() == 1);
}
