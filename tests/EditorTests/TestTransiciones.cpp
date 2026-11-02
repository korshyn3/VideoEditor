// TestTransiciones.cpp
// Escenarios de specs/clip-transitions (fase 7): agregar, cambiar y quitar transiciones
// (con el solapamiento de los clips y de sus audios vinculados), su invalidación al
// editar, la cadena xfade/acrossfade del grafo y el render real.
#include <doctest/doctest.h>

#include "EditorController/EditorController.h"
#include "EditorEngine/FilterGraphBuilder.h"
#include "EditorEngine/GraphRunner.h"
#include "EditorModel/Project.h"
#include "MediosPrueba.h"

using namespace model;
using engine::FilterGraphBuilder;

namespace {

TimeUs s(double segundos) { return segundosATiempo(segundos); }

MediaInfo videoFalso(const std::string& nombre, double segundos, bool audio = true) {
    MediaInfo m;
    m.path = nombre;
    m.duration = s(segundos);
    m.hasVideo = true;
    m.width = 1280;
    m.height = 720;
    m.fps = {25, 1};
    m.hasAudio = audio;
    m.sampleRate = 48000;
    m.channels = 2;
    return m;
}

// V1: A (10 s) 0–10 y B (5 s) 10–15, con su audio vinculado en "Audio V1".
struct DosClips {
    Project p;
    Id v1 = kSinId, a = kSinId, b = kSinId;

    explicit DosClips(bool audioA = true, bool audioB = true) {
        v1 = p.timeline().firstTrack(TrackKind::Video)->id;
        p.appendMedia(v1, p.addMedia(videoFalso("A.mp4", 10, audioA)), &a);
        p.appendMedia(v1, p.addMedia(videoFalso("B.mp4", 5, audioB)), &b);
    }
    const Clip& c(Id id) const { return *p.timeline().findClip(id); }
    const Track& pistaV1() const { return *p.timeline().findTrack(v1); }
    const Track& audioV1() const { return *p.timeline().audioTrackOf(v1); }
};

bool contiene(const std::string& txt, const std::string& sub) { return txt.find(sub) != std::string::npos; }

engine::TimelineSnapshot snapshot(const Project& p) { return {p.timeline(), p.media(), p.canvas()}; }

} // namespace

// ---------------------------------------------------------------------------
// Modelo
// ---------------------------------------------------------------------------

TEST_CASE("Transición de 1 segundo") {
    DosClips d;
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::Dissolve, s(1)));
    CHECK(d.c(d.b).timelineStart == s(9));  // B se adelanta 1 s
    CHECK(d.pistaV1().end() == s(14));
    REQUIRE(d.pistaV1().transitions.size() == 1);
    CHECK(d.pistaV1().transitions[0].toClipId == d.b);
    CHECK(d.p.transitionFrom(d.a)->duration == s(1));

    // Los audios vinculados se solapan igual y llevan la transición espejo.
    CHECK(d.c(d.c(d.b).linkedClipId).timelineStart == s(9));
    REQUIRE(d.audioV1().transitions.size() == 1);
    CHECK(d.audioV1().transitions[0].fromClipId == d.c(d.a).linkedClipId);
    CHECK(d.audioV1().transitions[0].duration == s(1));
}

TEST_CASE("Los clips siguientes también se adelantan") {
    DosClips d;
    Id c = kSinId;
    d.p.appendMedia(d.v1, d.p.addMedia(videoFalso("C.mp4", 4)), &c);  // C 15–19
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::WipeLeft, s(2)));
    CHECK(d.c(d.b).timelineStart == s(8));
    CHECK(d.c(c).timelineStart == s(13));
    CHECK(d.c(d.c(c).linkedClipId).timelineStart == s(13));
}

TEST_CASE("Clips no contiguos") {
    DosClips d;
    REQUIRE(d.p.moveClip(d.b, d.v1, s(12)));  // hueco de 2 s
    const OpResult r = d.p.addTransition(d.a, TransitionStyle::Dissolve, s(1));
    CHECK_FALSE(r.ok);
    CHECK(contiene(r.error, "contiguos"));
    CHECK(d.pistaV1().transitions.empty());
    CHECK(d.c(d.b).timelineStart == s(12));
}

TEST_CASE("Duración excesiva") {
    DosClips d;  // A dura 10 s y B 5 s
    CHECK(d.p.maxTransitionDuration(d.a) == s(5));
    const OpResult r = d.p.addTransition(d.a, TransitionStyle::Dissolve, s(6));
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.error.empty());
    CHECK(d.pistaV1().transitions.empty());
    CHECK_FALSE(d.p.addTransition(d.a, TransitionStyle::Dissolve, 0));  // debe ser > 0
    CHECK(d.p.addTransition(d.a, TransitionStyle::Dissolve, s(5)));     // justo el máximo
}

TEST_CASE("La otra transición de un clip reduce el máximo") {
    DosClips d;
    Id c = kSinId;
    d.p.appendMedia(d.v1, d.p.addMedia(videoFalso("C.mp4", 10)), &c);
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::Dissolve, s(2)));  // usa 2 s de B (5 s)
    CHECK(d.p.maxTransitionDuration(d.b) == s(3));
    CHECK_FALSE(d.p.addTransition(d.b, TransitionStyle::Dissolve, s(4)));
    CHECK(d.p.addTransition(d.b, TransitionStyle::Dissolve, s(3)));
}

TEST_CASE("Cambiar la duración") {
    DosClips d;
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::Dissolve, s(1)));
    REQUIRE(d.p.setTransition(d.a, TransitionStyle::Dissolve, s(2)));
    CHECK(d.c(d.b).timelineStart == s(8));
    CHECK(d.p.transitionFrom(d.a)->duration == s(2));
    CHECK(d.audioV1().transitions[0].duration == s(2));
}

TEST_CASE("Cambiar el estilo no mueve los clips") {
    DosClips d;
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::Dissolve, s(1)));
    REQUIRE(d.p.setTransition(d.a, TransitionStyle::SlideLeft, s(1)));
    CHECK(d.c(d.b).timelineStart == s(9));
    CHECK(d.p.transitionFrom(d.a)->style == TransitionStyle::SlideLeft);
}

TEST_CASE("Quitar la transición") {
    DosClips d;
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::Dissolve, s(1)));
    REQUIRE(d.p.removeTransition(d.a));
    CHECK(d.c(d.b).timelineStart == s(10));
    CHECK(d.c(d.b).timelineEnd() == s(15));
    CHECK(d.pistaV1().transitions.empty());
    CHECK(d.audioV1().transitions.empty());
    CHECK(d.c(d.c(d.b).linkedClipId).timelineStart == s(10));
    CHECK_FALSE(d.p.removeTransition(d.a));  // ya no hay
}

TEST_CASE("Eliminar un clip con transición") {
    DosClips d;
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::Dissolve, s(1)));
    REQUIRE(d.p.removeClip(d.b, false));
    CHECK(d.pistaV1().transitions.empty());
    CHECK(d.audioV1().transitions.empty());
    CHECK(d.p.transitionFrom(d.a) == nullptr);
}

TEST_CASE("Solo un clip con audio: no hay transición espejo") {
    DosClips d(true, false);  // B sin audio
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::Dissolve, s(1)));
    CHECK(d.audioV1().transitions.empty());
    CHECK(d.c(d.b).timelineStart == s(9));
}

TEST_CASE("Se rechaza si un audio vinculado se solaparía") {
    DosClips d;
    // El audio de A se desvincula: queda fijo en 0–10 y el audio de B no puede adelantarse.
    REQUIRE(d.p.unlinkAudio(d.a));
    const auto rev = d.p.revision();
    const OpResult r = d.p.addTransition(d.a, TransitionStyle::Dissolve, s(1));
    CHECK_FALSE(r.ok);
    CHECK(contiene(r.error, "Audio V1"));
    CHECK(d.p.revision() == rev);
    CHECK(d.c(d.b).timelineStart == s(10));
}

TEST_CASE("Alejar el clip quita la transición") {
    DosClips d;
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::Dissolve, s(1)));
    REQUIRE(d.p.moveClip(d.b, d.v1, s(20)));
    CHECK(d.pistaV1().transitions.empty());
    CHECK(d.audioV1().transitions.empty());
}

TEST_CASE("Recortar un clip de la transición cambia su duración o la quita") {
    DosClips d;
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::Dissolve, s(1)));  // A 0–10, B 9–14
    REQUIRE(d.p.trimClip(d.a, 0, s(9.5)));                              // A termina en 9.5
    REQUIRE(d.p.transitionFrom(d.a));
    CHECK(d.p.transitionFrom(d.a)->duration == s(0.5));
    REQUIRE(d.p.trimClip(d.a, 0, s(8)));  // ya no se solapan
    CHECK(d.p.transitionFrom(d.a) == nullptr);
    CHECK(d.audioV1().transitions.empty());
}

TEST_CASE("Dividir dentro de una transición se rechaza") {
    DosClips d;
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::Dissolve, s(1)));
    CHECK_FALSE(d.p.split(d.a, s(9.5)));
    CHECK_FALSE(d.p.split(d.b, s(9.5)));
    // Antes de la transición sí: la transición pasa a salir de la parte derecha de A.
    Id derecho = kSinId;
    REQUIRE(d.p.split(d.a, s(5), &derecho));
    REQUIRE(d.p.transitionFrom(derecho));
    CHECK(d.p.transitionFrom(derecho)->toClipId == d.b);
    CHECK(d.p.transitionFrom(d.a) == nullptr);
    CHECK(d.audioV1().transitions.size() == 1);
}

TEST_CASE("Mover juntos los dos clips conserva la transición") {
    DosClips d;
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::Dissolve, s(1)));
    REQUIRE(d.p.moveClips({d.a, d.b}, s(3)));
    REQUIRE(d.p.transitionFrom(d.a));
    CHECK(d.p.transitionFrom(d.a)->duration == s(1));
    CHECK(d.c(d.b).timelineStart == s(12));
}

TEST_CASE("Mover el clip dentro de la transición la alarga hasta el máximo") {
    DosClips d;
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::Dissolve, s(1)));
    // Mover B más adentro de A alarga la transición (siguen siendo los mismos clips)...
    REQUIRE(d.p.moveClip(d.b, d.v1, s(8)));
    CHECK(d.p.transitionFrom(d.a)->duration == s(2));
    // ...pero no más allá de lo que permite B (5 s).
    CHECK_FALSE(d.p.moveClip(d.b, d.v1, s(4)));
}

TEST_CASE("Controller: agregar transición desde el audio vinculado y quitarla") {
    controller::EditorController ctrl;
    const auto imp = ctrl.importFiles({medioPrueba("video_audio.mp4"), medioPrueba("video_audio.mp4")});
    REQUIRE(imp.clips.size() == 2);
    const Id audioA = ctrl.project().timeline().findClip(imp.clips[0])->linkedClipId;
    REQUIRE(ctrl.addTransition(audioA));  // se usa su video; duración por defecto 1 s
    CHECK(ctrl.seleccion().transicion == imp.clips[0]);
    CHECK(ctrl.seleccion().clips.empty());
    CHECK(ctrl.project().transitionFrom(imp.clips[0])->duration == s(1));
    REQUIRE(ctrl.removeSelectedTransition());
    CHECK(ctrl.seleccion().transicion == kSinId);
    CHECK(ctrl.project().timeline().findClip(imp.clips[1])->timelineStart == s(5));
}

// ---------------------------------------------------------------------------
// Cadena de filtros
// ---------------------------------------------------------------------------

TEST_CASE("FilterGraphBuilder: xfade y acrossfade entre los clips") {
    DosClips d;
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::Circle, s(1)));
    const auto g = FilterGraphBuilder::build(snapshot(d.p), {640, 360, {25, 1}}, 0);
    CHECK(g.fin == s(14));
    CHECK(contiene(g.filtros, "xfade=transition=circleopen:duration=1.000000:offset=9.000000"));
    CHECK(contiene(g.filtros, "acrossfade=d=1.000000"));
    CHECK(contiene(g.filtros, "trim=end_frame=350"));  // la cadena: 14 s a 25 fps
    CHECK_FALSE(contiene(g.filtros, "concat=n=2:v=1"));  // una sola cadena, sin concat
}

TEST_CASE("FilterGraphBuilder: estilos de xfade") {
    const std::pair<TransitionStyle, const char*> estilos[] = {
        {TransitionStyle::Dissolve, "transition=fade:"},       {TransitionStyle::WipeLeft, "transition=wipeleft:"},
        {TransitionStyle::WipeRight, "transition=wiperight:"}, {TransitionStyle::SlideLeft, "transition=slideleft:"},
        {TransitionStyle::SlideRight, "transition=slideright:"}, {TransitionStyle::Circle, "transition=circleopen:"},
    };
    for (const auto& [estilo, texto] : estilos) {
        DosClips d;
        REQUIRE(d.p.addTransition(d.a, estilo, s(1)));
        CHECK(contiene(FilterGraphBuilder::build(snapshot(d.p), {640, 360, {25, 1}}, 0).filtros, texto));
    }
}

TEST_CASE("FilterGraphBuilder: solo un clip con audio hace afade") {
    DosClips d(true, false);
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::Dissolve, s(1)));
    const auto g = FilterGraphBuilder::build(snapshot(d.p), {640, 360, {25, 1}}, 0);
    CHECK_FALSE(contiene(g.filtros, "acrossfade"));
    CHECK(contiene(g.filtros, "afade=t=out:st=9.000000:d=1.000000"));
}

TEST_CASE("FilterGraphBuilder: arranque dentro de una transición") {
    DosClips d;
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::Dissolve, s(1)));  // transición 9–10 s
    const auto g = FilterGraphBuilder::build(snapshot(d.p), {640, 360, {25, 1}}, s(9.5));
    CHECK(g.t0 == s(9.48));  // 9.5 s alineado hacia abajo a la rejilla de 25 fps
    // El grafo arranca en 9 s (inicio de la transición) y descarta lo anterior a 9.5 s.
    CHECK(contiene(g.filtros, "xfade=transition=fade:duration=1.000000:offset=0.000000"));
    CHECK(contiene(g.filtros, "trim=start_frame=12"));  // 0.5 s a 25 fps (redondeado al cuadro)
    CHECK(contiene(g.filtros, "atrim=start_sample="));
    CHECK(contiene(g.filtros, "setpts=round(N*1/25/TB+9.480000/TB)[vout]"));  // cuadro 237
}

TEST_CASE("FilterGraphBuilder: arranque después de la transición no la incluye") {
    DosClips d;
    REQUIRE(d.p.addTransition(d.a, TransitionStyle::Dissolve, s(1)));
    const auto g = FilterGraphBuilder::build(snapshot(d.p), {640, 360, {25, 1}}, s(11));
    CHECK_FALSE(contiene(g.filtros, "xfade"));
    CHECK_FALSE(contiene(g.filtros, "acrossfade"));
    CHECK_FALSE(contiene(g.filtros, "trim=start_frame"));
}

// ---------------------------------------------------------------------------
// Render real
// ---------------------------------------------------------------------------

namespace {

struct Conteo {
    int cuadros = 0;
    long long muestras = 0;
    TimeUs primerPts = -1;
};

Conteo renderizar(const engine::GraphDescription& d) {
    Conteo c;
    engine::GraphRunner runner;
    std::string err;
    REQUIRE_MESSAGE(runner.build(d, &err), err);
    AVFrame* f = av_frame_alloc();
    TimeUs pts = 0;
    int r;
    while ((r = runner.pullVideo(f, &pts)) > 0) {
        if (c.primerPts < 0) c.primerPts = pts;
        ++c.cuadros;
        av_frame_unref(f);
    }
    CHECK_MESSAGE(r == 0, runner.ultimoError());
    while ((r = runner.pullAudio(f, &pts)) > 0) {
        c.muestras += f->nb_samples;
        av_frame_unref(f);
    }
    CHECK_MESSAGE(r == 0, runner.ultimoError());
    av_frame_free(&f);
    return c;
}

} // namespace

TEST_CASE("GraphRunner: la salida dura la + lb - D") {
    controller::EditorController ctrl;
    const auto imp = ctrl.importFiles({medioPrueba("video_audio.mp4"), medioPrueba("video_audio.mp4")});
    REQUIRE(imp.clips.size() == 2);
    REQUIRE(ctrl.addTransition(imp.clips[0], TransitionStyle::WipeLeft, s(1)));
    const auto snap = snapshot(ctrl.project());
    const engine::RenderSettings rs{320, 240, {25, 1}};

    const Conteo c = renderizar(FilterGraphBuilder::build(snap, rs, 0));
    CHECK(c.cuadros == 225);              // (5 + 5 - 1) s a 25 fps
    CHECK(c.muestras == 9 * 48000);
    CHECK(c.primerPts == 0);

    // Arrancando dentro de la transición, el primer cuadro es el pedido.
    const Conteo dentro = renderizar(FilterGraphBuilder::build(snap, rs, s(4.6)));
    CHECK(dentro.primerPts == s(4.6));
    CHECK(dentro.cuadros == 225 - 115);
    CHECK(dentro.muestras == doctest::Approx(4.4 * 48000).epsilon(0.001));
}

TEST_CASE("GraphRunner: transición con un solo clip con audio") {
    controller::EditorController ctrl;
    const auto imp = ctrl.importFiles({medioPrueba("video_audio.mp4"), medioPrueba("solo_video.mp4")});
    REQUIRE(imp.clips.size() == 2);
    REQUIRE(ctrl.addTransition(imp.clips[0], TransitionStyle::Dissolve, s(1)));
    const Conteo c = renderizar(FilterGraphBuilder::build(snapshot(ctrl.project()), {320, 240, {25, 1}}, 0));
    CHECK(c.cuadros == 175);  // (5 + 3 - 1) s
    CHECK(c.muestras == 7 * 48000);
}
