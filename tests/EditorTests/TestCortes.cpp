// TestCortes.cpp
// Escenarios de specs/timeline-editing de la fase 4: dividir (split), recortar (trim) y
// eliminar clips (con hueco o con ripple), más la edición no destructiva.
#include <doctest/doctest.h>

#include "EditorController/EditorController.h"
#include "EditorModel/Project.h"
#include "MediosPrueba.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

using namespace model;

namespace {

MediaInfo videoFalso(const std::string& nombre, double segundos) {
    MediaInfo m;
    m.path = nombre;
    m.duration = segundosATiempo(segundos);
    m.hasVideo = true;
    m.width = 1280;
    m.height = 720;
    m.fps = {25, 1};
    m.videoCodec = "h264";
    m.hasAudio = true;
    m.sampleRate = 48000;
    m.channels = 2;
    m.audioCodec = "aac";
    return m;
}

TimeUs s(double segundos) { return segundosATiempo(segundos); }

// Proyecto con A (10 s), B (5 s) y C (8 s) seguidos en V1: A 0–10, B 10–15, C 15–23.
struct TresClips {
    Project p;
    Id v1 = kSinId, a = kSinId, b = kSinId, c = kSinId;

    TresClips() {
        v1 = p.timeline().firstTrack(TrackKind::Video)->id;
        p.appendMedia(v1, p.addMedia(videoFalso("A.mp4", 10)), &a);
        p.appendMedia(v1, p.addMedia(videoFalso("B.mp4", 5)), &b);
        p.appendMedia(v1, p.addMedia(videoFalso("C.mp4", 8)), &c);
    }
    const Clip& clip(Id id) const { return *p.timeline().findClip(id); }
    const Track& pistaV1() const { return *p.timeline().findTrack(v1); }
};

} // namespace

// ---------------------------------------------------------------------------
// Split
// ---------------------------------------------------------------------------

TEST_CASE("Split en el medio") {
    TresClips t;
    Id derecho = kSinId;
    REQUIRE(t.p.split(t.a, s(4), &derecho));

    const Clip& izq = t.clip(t.a);
    const Clip& der = t.clip(derecho);
    CHECK(izq.timelineStart == 0);
    CHECK(izq.timelineEnd() == s(4));
    CHECK(izq.sourceIn == 0);
    CHECK(izq.sourceOut == s(4));
    CHECK(der.timelineStart == s(4));
    CHECK(der.timelineEnd() == s(10));
    CHECK(der.sourceIn == s(4));
    CHECK(der.sourceOut == s(10));
    CHECK(der.mediaId == izq.mediaId);

    // Los clips siguen ordenados y el resto del timeline no cambió.
    const Track& v1 = t.pistaV1();
    REQUIRE(v1.clips.size() == 4);
    CHECK(v1.clips[0].id == t.a);
    CHECK(v1.clips[1].id == derecho);
    CHECK(t.clip(t.b).timelineStart == s(10));
    CHECK(t.p.timeline().duration() == s(23));
}

TEST_CASE("Split de un clip ya recortado usa el tramo de la fuente") {
    TresClips t;
    REQUIRE(t.p.trimClip(t.c, s(2), s(8)));  // C: fuente 2–8, timeline 17–23
    Id derecho = kSinId;
    REQUIRE(t.p.split(t.c, s(20), &derecho));
    CHECK(t.clip(t.c).sourceOut == s(5));
    CHECK(t.clip(derecho).sourceIn == s(5));
    CHECK(t.clip(derecho).sourceOut == s(8));
}

TEST_CASE("Cursor fuera del clip") {
    TresClips t;
    const auto rev = t.p.revision();
    CHECK_FALSE(t.p.split(t.b, s(4)).ok);   // fuera
    CHECK_FALSE(t.p.split(t.b, s(10)).ok);  // borde izquierdo
    CHECK_FALSE(t.p.split(t.b, s(15)).ok);  // borde derecho
    CHECK(t.p.revision() == rev);
    CHECK(t.pistaV1().clips.size() == 3);
}

TEST_CASE("Split conserva el audio vinculado") {
    TresClips t;
    Id derecho = kSinId;
    REQUIRE(t.clip(t.b).linkedAudio);
    REQUIRE(t.p.split(t.b, s(12), &derecho));
    CHECK(t.clip(t.b).linkedAudio);
    CHECK(t.clip(derecho).linkedAudio);
}

// ---------------------------------------------------------------------------
// Trim
// ---------------------------------------------------------------------------

TEST_CASE("Recortar el inicio") {
    TresClips t;
    // Arrastrar el borde izquierdo de B 2 s a la derecha = punto de entrada +2 s.
    REQUIRE(t.p.trimClip(t.b, s(2), s(5)));
    CHECK(t.clip(t.b).timelineStart == s(12));  // empieza 2 s más tarde
    CHECK(t.clip(t.b).sourceIn == s(2));        // la entrada en la fuente avanza 2 s
    CHECK(t.clip(t.b).timelineEnd() == s(15));  // el final no se mueve
}

TEST_CASE("Recortar el final") {
    TresClips t;
    REQUIRE(t.p.trimClip(t.c, 0, s(6)));
    CHECK(t.clip(t.c).timelineStart == s(15));
    CHECK(t.clip(t.c).timelineEnd() == s(21));
    CHECK(t.p.timeline().duration() == s(21));
}

TEST_CASE("Extender más allá de la fuente") {
    TresClips t;
    REQUIRE(t.p.trimClip(t.c, 0, s(5)));    // C: 15–20 (fuente 0–5)
    REQUIRE(t.p.trimClip(t.c, 0, s(100)));  // pide 100 s pero la fuente dura 8
    CHECK(t.clip(t.c).sourceOut == s(8));
    CHECK(t.clip(t.c).timelineEnd() == s(23));
    // Tampoco se puede retroceder la entrada antes del inicio del archivo.
    REQUIRE(t.p.trimClip(t.c, s(-3), s(8)));
    CHECK(t.clip(t.c).sourceIn == 0);
}

TEST_CASE("Trim hasta tocar a los vecinos") {
    TresClips t;
    REQUIRE(t.p.trimClip(t.b, s(1), s(4)));  // B: fuente 1–4, timeline 11–14
    // Extender la entrada hasta 0 hace empezar B en 10, justo donde termina A: cabe.
    REQUIRE(t.p.trimClip(t.b, 0, s(4)));
    CHECK(t.clip(t.b).timelineStart == s(10));
    // Extender la salida hasta 5 hace terminar B en 15, justo donde empieza C: cabe.
    REQUIRE(t.p.trimClip(t.b, 0, s(5)));
    CHECK(t.clip(t.b).timelineEnd() == s(15));
}

TEST_CASE("Trim que solaparía se rechaza sin cambios") {
    TresClips t;
    REQUIRE(t.p.trimClip(t.b, s(2), s(5)));  // B: 12–15, deja hueco 10–12
    REQUIRE(t.p.moveClip(t.b, t.v1, s(11)));  // B: 11–14
    const auto rev = t.p.revision();
    // Entrada a 0 haría empezar B en 9 (solapa A, que termina en 10).
    CHECK_FALSE(t.p.trimClip(t.b, 0, s(5)).ok);
    CHECK(t.p.revision() == rev);
    CHECK(t.clip(t.b).timelineStart == s(11));
    CHECK(t.clip(t.b).sourceIn == s(2));
}

TEST_CASE("Trim de al menos un cuadro") {
    TresClips t;  // lienzo a 25 fps: 1 cuadro = 40 ms
    CHECK(t.p.frameDuration() == 40'000);
    CHECK_FALSE(t.p.trimClip(t.b, s(1), s(1) + 39'999).ok);
    CHECK(t.p.trimClip(t.b, s(1), s(1) + 40'000).ok);
}

// ---------------------------------------------------------------------------
// Eliminar
// ---------------------------------------------------------------------------

TEST_CASE("Eliminar dejando hueco") {
    TresClips t;
    REQUIRE(t.p.removeClip(t.b, false));
    CHECK(t.p.timeline().findClip(t.b) == nullptr);
    CHECK(t.clip(t.c).timelineStart == s(15));  // hueco de 10 a 15 s
    CHECK(t.p.timeline().duration() == s(23));
}

TEST_CASE("Eliminar con ripple") {
    TresClips t;
    REQUIRE(t.p.removeClip(t.b, true));
    CHECK(t.p.timeline().findClip(t.b) == nullptr);
    CHECK(t.clip(t.a).timelineStart == 0);      // lo anterior no se mueve
    CHECK(t.clip(t.c).timelineStart == s(10));  // C pasa a empezar en 10 s
    CHECK(t.p.timeline().duration() == s(18));
}

TEST_CASE("El ripple solo afecta a la misma pista") {
    TresClips t;
    Id v2 = kSinId, enV2 = kSinId;
    t.p.addTrack(TrackKind::Video, &v2);
    t.p.appendMedia(v2, t.p.addMedia(videoFalso("D.mp4", 3)), &enV2);
    REQUIRE(t.p.moveClip(enV2, v2, s(16)));
    REQUIRE(t.p.removeClip(t.b, true));
    CHECK(t.clip(enV2).timelineStart == s(16));
}

TEST_CASE("Eliminar un segmento intermedio") {
    Project p;
    const Id v1 = p.timeline().firstTrack(TrackKind::Video)->id;
    Id clip = kSinId, medio = kSinId, ultimo = kSinId;
    p.appendMedia(v1, p.addMedia(videoFalso("A.mp4", 10)), &clip);
    REQUIRE(p.split(clip, s(3), &medio));
    REQUIRE(p.split(medio, s(6), &ultimo));
    REQUIRE(p.removeClip(medio, true));

    // Fuente 0–3 s seguida inmediatamente de la fuente 6–10 s.
    const Track& t = *p.timeline().findTrack(v1);
    REQUIRE(t.clips.size() == 2);
    CHECK(t.clips[0].sourceIn == 0);
    CHECK(t.clips[0].sourceOut == s(3));
    CHECK(t.clips[1].timelineStart == s(3));
    CHECK(t.clips[1].sourceIn == s(6));
    CHECK(t.clips[1].sourceOut == s(10));
    CHECK(p.timeline().duration() == s(7));
}

TEST_CASE("Eliminar un clip inexistente") {
    TresClips t;
    const auto rev = t.p.revision();
    CHECK_FALSE(t.p.removeClip(9999, false).ok);
    CHECK(t.p.revision() == rev);
}

// ---------------------------------------------------------------------------
// Controller: acciones sobre la selección y el cursor
// ---------------------------------------------------------------------------

TEST_CASE("Controller: dividir en el cursor y eliminar la selección") {
    controller::EditorController ctrl;
    const auto r = ctrl.importFiles({medioPrueba("video_audio.mp4")});  // 5 s
    REQUIRE(r.errores.empty());
    const Id clip = r.clips.at(0);

    CHECK_FALSE(ctrl.splitAtPlayhead().ok);  // sin clip seleccionado
    ctrl.seleccionarClip(clip);
    ctrl.setPlayhead(s(2));
    REQUIRE(ctrl.splitAtPlayhead());
    CHECK(ctrl.seleccion().clipId == clip);  // la selección queda en la parte izquierda
    const Track* v1 = ctrl.project().timeline().firstTrack(TrackKind::Video);
    REQUIRE(v1->clips.size() == 2);

    REQUIRE(ctrl.deleteSelectedClip(true));
    CHECK(ctrl.seleccion().clipId == kSinId);
    REQUIRE(v1->clips.size() == 1);
    CHECK(v1->clips[0].timelineStart == 0);
    CHECK(v1->clips[0].sourceIn == s(2));
}

// ---------------------------------------------------------------------------
// Edición no destructiva
// ---------------------------------------------------------------------------

namespace {

struct Huella {
    std::uint64_t hash = 0;
    std::filesystem::file_time_type modificado;
    std::uintmax_t tamano = 0;
    bool operator==(const Huella&) const = default;
};

// FNV-1a de 64 bits sobre el contenido completo del archivo.
Huella huellaDe(const std::string& rutaUtf8) {
    const std::filesystem::path ruta(std::u8string(rutaUtf8.begin(), rutaUtf8.end()));
    std::ifstream f(ruta, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    Huella h;
    h.hash = 1469598103934665603ull;
    for (char c : bytes) {
        h.hash ^= static_cast<unsigned char>(c);
        h.hash *= 1099511628211ull;
    }
    h.modificado = std::filesystem::last_write_time(ruta);
    h.tamano = std::filesystem::file_size(ruta);
    return h;
}

} // namespace

TEST_CASE("Edición no destructiva") {
    const std::vector<std::string> medios = {medioPrueba("video_audio.mp4"), medioPrueba("solo_video.mp4")};
    std::vector<Huella> antes;
    for (const std::string& m : medios) antes.push_back(huellaDe(m));

    {
        controller::EditorController ctrl;
        const auto r = ctrl.importFiles(medios);
        REQUIRE(r.errores.empty());
        // Split, trim, mover y eliminar sobre los clips importados.
        ctrl.seleccionarClip(r.clips[0]);
        ctrl.setPlayhead(s(2.5));
        REQUIRE(ctrl.splitAtPlayhead());
        REQUIRE(ctrl.trimClip(r.clips[0], s(0.5), s(2.5)));
        ctrl.seleccionarClip(r.clips[1]);
        REQUIRE(ctrl.deleteSelectedClip(true));
        REQUIRE(ctrl.removeTrack(ctrl.project().timeline().firstTrack(TrackKind::Audio)->id));
    }

    for (size_t i = 0; i < medios.size(); ++i) {
        CAPTURE(medios[i]);
        CHECK(huellaDe(medios[i]) == antes[i]);
    }
}
