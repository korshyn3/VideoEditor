// TestTimelineEditing.cpp
// Escenarios de specs/timeline-editing (fase 2: estructura, pistas, lienzo, colocación
// en orden y movimiento de clips).
#include <doctest/doctest.h>

#include "EditorModel/Project.h"

using namespace model;

namespace {

MediaInfo videoFalso(const std::string& nombre, double segundos, int w = 1280, int h = 720, Rational fps = {25, 1},
                     bool conAudio = true) {
    MediaInfo m;
    m.path = nombre;
    m.duration = segundosATiempo(segundos);
    m.hasVideo = true;
    m.width = w;
    m.height = h;
    m.fps = fps;
    m.videoCodec = "h264";
    m.hasAudio = conAudio;
    if (conAudio) {
        m.sampleRate = 48000;
        m.channels = 2;
        m.audioCodec = "aac";
    }
    return m;
}

MediaInfo audioFalso(const std::string& nombre, double segundos) {
    MediaInfo m;
    m.path = nombre;
    m.duration = segundosATiempo(segundos);
    m.hasAudio = true;
    m.sampleRate = 44100;
    m.channels = 2;
    m.audioCodec = "mp3";
    return m;
}

Id pista(const Project& p, TrackKind kind) {
    return p.timeline().firstTrack(kind)->id;
}

} // namespace

TEST_CASE("Proyecto nuevo") {
    Project p;
    const auto& tracks = p.timeline().tracks;
    REQUIRE(tracks.size() == 3);
    CHECK(tracks[0].kind == TrackKind::Video);
    CHECK(tracks[0].name == "V1");
    CHECK(tracks[1].esAudioDeVideo());
    CHECK(tracks[1].name == "Audio V1");
    CHECK(tracks[1].videoTrackId == tracks[0].id);
    CHECK(tracks[2].esAudioGeneral());
    CHECK(tracks[2].name == "A1");
    CHECK(p.timeline().empty());
    CHECK_FALSE(p.dirty());
}

TEST_CASE("Agregar pistas") {
    Project p;
    Id v2 = kSinId, a2 = kSinId;
    REQUIRE(p.addTrack(TrackKind::Video, &v2));
    REQUIRE(p.addTrack(TrackKind::Audio, &a2));
    CHECK(p.timeline().findTrack(v2)->name == "V2");
    CHECK(p.timeline().findTrack(a2)->name == "A2");
    CHECK(p.timeline().findTrack(v2)->clips.empty());
    // Orden: las pistas de video van juntas (V1, V2) y antes que las de audio.
    CHECK(p.timeline().tracks[1].id == v2);
    CHECK(p.dirty());
}

TEST_CASE("Nombres correlativos sin repetir tras borrar") {
    Project p;
    Id v2 = kSinId, v3 = kSinId;
    p.addTrack(TrackKind::Video, &v2);
    p.addTrack(TrackKind::Video, &v3);
    REQUIRE(p.removeTrack(v2));
    Id v4 = kSinId;
    p.addTrack(TrackKind::Video, &v4);
    CHECK(p.timeline().findTrack(v4)->name == "V4");
}

TEST_CASE("Eliminar la única pista de video") {
    Project p;
    const auto r = p.removeTrack(pista(p, TrackKind::Video));
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.error.empty());
    CHECK(p.timeline().countTracks(TrackKind::Video) == 1);
}

TEST_CASE("Eliminar pista con sus clips") {
    Project p;
    Id a1 = p.timeline().firstGeneralAudioTrack()->id;
    Id m = p.addMedia(audioFalso("musica.mp3", 60));
    REQUIRE(p.appendMedia(a1, m));
    REQUIRE(p.removeTrack(a1));
    CHECK(p.timeline().findTrack(a1) == nullptr);
    CHECK(p.timeline().empty());
}

TEST_CASE("Primer video define el lienzo") {
    Project p;
    Id m = p.addMedia(videoFalso("a.mp4", 10, 1280, 720, {25, 1}));
    REQUIRE(p.appendMedia(pista(p, TrackKind::Video), m));
    CHECK(p.canvas().width == 1280);
    CHECK(p.canvas().height == 720);
    CHECK(p.canvas().fps.num == 25);
    CHECK(p.canvas().fps.den == 1);

    // Un segundo video con otra resolución NO cambia el lienzo.
    Id m2 = p.addMedia(videoFalso("b.mp4", 5, 640, 480, {30, 1}));
    REQUIRE(p.appendMedia(pista(p, TrackKind::Video), m2));
    CHECK(p.canvas().width == 1280);

    // El usuario sí lo puede cambiar después.
    REQUIRE(p.setCanvas({1920, 1080, {30, 1}}));
    CHECK(p.canvas().width == 1920);
    CHECK_FALSE(p.setCanvas({0, 1080, {30, 1}}).ok);
}

TEST_CASE("Importar tres videos") {
    Project p;
    const Id v1 = pista(p, TrackKind::Video);
    Id ca = kSinId, cb = kSinId, cc = kSinId;
    REQUIRE(p.appendMedia(v1, p.addMedia(videoFalso("A.mp4", 10)), &ca));
    REQUIRE(p.appendMedia(v1, p.addMedia(videoFalso("B.mp4", 5)), &cb));
    REQUIRE(p.appendMedia(v1, p.addMedia(videoFalso("C.mp4", 8)), &cc));

    const Track* t = p.timeline().findTrack(v1);
    REQUIRE(t->clips.size() == 3);
    CHECK(t->findClip(ca)->timelineStart == 0);
    CHECK(t->findClip(ca)->timelineEnd() == segundosATiempo(10));
    CHECK(t->findClip(cb)->timelineStart == segundosATiempo(10));
    CHECK(t->findClip(cb)->timelineEnd() == segundosATiempo(15));
    CHECK(t->findClip(cc)->timelineStart == segundosATiempo(15));
    CHECK(t->findClip(cc)->timelineEnd() == segundosATiempo(23));
    // Cada clip abarca el archivo completo y su audio queda vinculado.
    CHECK(t->findClip(cb)->sourceIn == 0);
    CHECK(t->findClip(cb)->sourceOut == segundosATiempo(5));
    CHECK(t->findClip(cb)->vinculado());
    CHECK(p.timeline().duration() == segundosATiempo(23));
}

TEST_CASE("Tipo de pista incompatible") {
    Project p;
    CHECK_FALSE(p.appendMedia(pista(p, TrackKind::Video), p.addMedia(audioFalso("m.mp3", 3))).ok);
    CHECK_FALSE(p.appendMedia(pista(p, TrackKind::Audio),
                              p.addMedia(videoFalso("mudo.mp4", 3, 640, 360, {30, 1}, false))).ok);
}

TEST_CASE("Mover a un hueco libre") {
    Project p;
    const Id v1 = pista(p, TrackKind::Video);
    Id v2 = kSinId;
    p.addTrack(TrackKind::Video, &v2);
    Id clip = kSinId;
    REQUIRE(p.appendMedia(v1, p.addMedia(videoFalso("A.mp4", 10)), &clip));

    const auto rev = p.revision();
    REQUIRE(p.moveClip(clip, v2, segundosATiempo(3)));
    CHECK(p.revision() > rev);
    CHECK(p.timeline().findTrack(v1)->clips.empty());
    const Clip* c = p.timeline().findTrack(v2)->findClip(clip);
    REQUIRE(c != nullptr);
    CHECK(c->timelineStart == segundosATiempo(3));
}

TEST_CASE("Movimiento con solapamiento") {
    Project p;
    const Id v1 = pista(p, TrackKind::Video);
    Id ca = kSinId, cb = kSinId;
    p.appendMedia(v1, p.addMedia(videoFalso("A.mp4", 10)), &ca);
    p.appendMedia(v1, p.addMedia(videoFalso("B.mp4", 5)), &cb);

    const auto rev = p.revision();
    const auto r = p.moveClip(cb, v1, segundosATiempo(8));  // B quedaría encima de A
    CHECK_FALSE(r.ok);
    CHECK(p.revision() == rev);  // nada cambió
    CHECK(p.timeline().findClip(cb)->timelineStart == segundosATiempo(10));
}

TEST_CASE("Mover a una pista de otro tipo se rechaza") {
    Project p;
    Id clip = kSinId;
    p.appendMedia(pista(p, TrackKind::Video), p.addMedia(videoFalso("A.mp4", 10)), &clip);
    CHECK_FALSE(p.moveClip(clip, pista(p, TrackKind::Audio), 0).ok);
}
