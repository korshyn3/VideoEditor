// TestPip.cpp
// Escenarios de specs/video-compositing (fase 8): transformación de los clips de video,
// su traducción a filtros (overlay) y el render real.
#include <doctest/doctest.h>

#include "EditorController/EditorController.h"
#include "EditorEngine/FilterGraphBuilder.h"
#include "EditorEngine/GraphRunner.h"
#include "EditorModel/Project.h"
#include "MediosPrueba.h"

#include <cstdint>
#include <vector>

using namespace model;
using engine::FilterGraphBuilder;

namespace {

TimeUs s(double segundos) { return segundosATiempo(segundos); }

MediaInfo videoFalso(const std::string& nombre, double segundos) {
    MediaInfo m;
    m.path = nombre;
    m.duration = s(segundos);
    m.hasVideo = true;
    m.width = 1280;
    m.height = 720;
    m.fps = {25, 1};
    return m;
}

// V1 con A (10 s) y V2 con B (4 s).
struct DosPistas {
    Project p;
    Id v1 = kSinId, v2 = kSinId, a = kSinId, b = kSinId;

    DosPistas() {
        v1 = p.timeline().firstTrack(TrackKind::Video)->id;
        p.addTrack(TrackKind::Video, &v2);
        p.appendMedia(v1, p.addMedia(videoFalso("A.mp4", 10)), &a);
        p.appendMedia(v2, p.addMedia(videoFalso("B.mp4", 4)), &b);
    }
    const Clip& c(Id id) const { return *p.timeline().findClip(id); }
};

bool contiene(const std::string& txt, const std::string& sub) { return txt.find(sub) != std::string::npos; }

engine::TimelineSnapshot snapshot(const Project& p) { return {p.timeline(), p.media(), p.canvas()}; }

} // namespace

// ---------------------------------------------------------------------------
// Modelo
// ---------------------------------------------------------------------------

TEST_CASE("PiP por defecto") {
    DosPistas d;
    const PipTransform& t = d.c(d.b).transform;
    CHECK(t.x == 0.0);
    CHECK(t.y == 0.0);
    CHECK(t.scale == 1.0);
    CHECK(t.opacity == 1.0);
}

TEST_CASE("Los valores del PiP fuera de rango se limitan") {
    DosPistas d;
    REQUIRE(d.p.setPipTransform(d.b, {-1.0, 2.0, 5.0, 1.5}));
    const PipTransform& t = d.c(d.b).transform;
    CHECK(t.x == 0.0);
    CHECK(t.y == 1.0);
    CHECK(t.scale == 1.0);
    CHECK(t.opacity == 1.0);
    REQUIRE(d.p.setPipTransform(d.b, {0.5, 0.5, 0.01, -0.3}));
    CHECK(d.c(d.b).transform.scale == PipTransform::kEscalaMin);
    CHECK(d.c(d.b).transform.opacity == 0.0);
}

TEST_CASE("Cambiar el tamaño en cualquier pista de video") {
    DosPistas d;
    REQUIRE(d.p.setPipTransform(d.a, {0.25, 0.25, 0.5, 1}));  // A está en V1
    CHECK(d.c(d.a).transform == PipTransform{0.25, 0.25, 0.5, 1});

    // Un clip de audio no tiene tamaño.
    MediaInfo conAudio = videoFalso("C.mp4", 2);
    conAudio.hasAudio = true;
    Id c = kSinId;
    REQUIRE(d.p.appendMedia(d.v1, d.p.addMedia(conAudio), &c));
    const auto rev = d.p.revision();
    CHECK_FALSE(d.p.setPipTransform(d.c(c).linkedClipId, {0, 0, 0.5, 1}));
    CHECK(d.p.revision() == rev);
}

TEST_CASE("Mover un clip de pista conserva su tamaño") {
    DosPistas d;
    REQUIRE(d.p.setPipTransform(d.b, {0.1, 0.2, 0.4, 0.5}));
    REQUIRE(d.p.moveClip(d.b, d.v2, s(5)));
    CHECK(d.c(d.b).transform == PipTransform{0.1, 0.2, 0.4, 0.5});
    REQUIRE(d.p.moveClip(d.b, d.v1, s(20)));
    CHECK(d.c(d.b).transform == PipTransform{0.1, 0.2, 0.4, 0.5});
}

// ---------------------------------------------------------------------------
// Cadena de filtros
// ---------------------------------------------------------------------------

TEST_CASE("FilterGraphBuilder: PiP sobre V1") {
    DosPistas d;
    REQUIRE(d.p.setPipTransform(d.b, {0.65, 0.65, 0.3, 1}));
    const auto g = FilterGraphBuilder::build(snapshot(d.p), {640, 360, {25, 1}}, 0);
    REQUIRE_FALSE(g.vacio);
    CHECK(contiene(g.filtros, "scale=192:108"));              // 30 % de 640x360
    CHECK(contiene(g.filtros, "pad=192:108"));
    CHECK(contiene(g.filtros, "overlay=x=416:y=234"));        // 0.65 · 640, 0.65 · 360
    CHECK(contiene(g.filtros, "format=yuva420p"));
    CHECK(contiene(g.filtros, "color=c=black@0.0:s=640x360"));  // V2 después de B: transparente
    CHECK(contiene(g.filtros, "overlay=x=0:y=0:eof_action=pass,format=yuv420p"));
    CHECK_FALSE(contiene(g.filtros, "lutyuv=a="));  // opacidad 1: sin cambio de alfa
}

TEST_CASE("FilterGraphBuilder: opacidad y efectos del PiP") {
    DosPistas d;
    REQUIRE(d.p.setPipTransform(d.b, {0, 0, 0.5, 0.5}));
    REQUIRE(d.p.addEffect(d.b, Grayscale{}));
    const auto g = FilterGraphBuilder::build(snapshot(d.p), {640, 360, {25, 1}}, 0);
    CHECK(contiene(g.filtros, "scale=320:180"));
    CHECK(contiene(g.filtros, "overlay=x=0:y=0:eof_action=pass[v2s0]"));
    CHECK(contiene(g.filtros, "lutyuv=a='val*0.5000'"));
    // El efecto va dentro de la cadena del PiP, antes de pasar a yuva420p.
    const auto hue = g.filtros.find("hue=s=0");
    REQUIRE(hue != std::string::npos);
    CHECK(hue < g.filtros.find("format=yuva420p"));
}

TEST_CASE("FilterGraphBuilder: tamaño de un clip de V1") {
    DosPistas d;
    REQUIRE(d.p.removeClip(d.b, false));
    // Sin transformación, ocupa el lienzo sin overlay.
    auto g = FilterGraphBuilder::build(snapshot(d.p), {640, 360, {25, 1}}, 0);
    CHECK_FALSE(contiene(g.filtros, "overlay"));

    // Con transformación, sobre el fondo negro y sin dejar de ser yuv420p.
    REQUIRE(d.p.setPipTransform(d.a, {0.25, 0.25, 0.5, 1}));
    g = FilterGraphBuilder::build(snapshot(d.p), {640, 360, {25, 1}}, 0);
    CHECK(contiene(g.filtros, "scale=320:180"));
    CHECK(contiene(g.filtros, "color=c=black:s=640x360"));
    CHECK(contiene(g.filtros, "overlay=x=160:y=90:eof_action=pass,format=yuv420p"));
}

TEST_CASE("FilterGraphBuilder: una pista superior vacía no se compone") {
    Project p;
    const Id v1 = p.timeline().firstTrack(TrackKind::Video)->id;
    p.addTrack(TrackKind::Video);
    p.appendMedia(v1, p.addMedia(videoFalso("A.mp4", 10)));
    const auto g = FilterGraphBuilder::build(snapshot(p), {640, 360, {25, 1}}, 0);
    CHECK_FALSE(contiene(g.filtros, "overlay"));
}

TEST_CASE("FilterGraphBuilder: solo la pista superior tiene clip") {
    Project p;
    Id v2 = kSinId;
    p.addTrack(TrackKind::Video, &v2);
    p.appendMedia(v2, p.addMedia(videoFalso("B.mp4", 4)));
    const auto g = FilterGraphBuilder::build(snapshot(p), {640, 360, {25, 1}}, 0);
    REQUIRE_FALSE(g.vacio);
    CHECK(contiene(g.filtros, "color=c=black:s="));  // V1: fondo negro
    CHECK(contiene(g.filtros, "overlay=x=0:y=0:eof_action=pass"));
}

// ---------------------------------------------------------------------------
// Render real
// ---------------------------------------------------------------------------

namespace {

// Suma del plano Y de cada cuadro (para comparar cuadros entre sí).
std::vector<std::uint64_t> sumasDeCuadros(const engine::GraphDescription& d, int* ancho = nullptr) {
    std::vector<std::uint64_t> sumas;
    engine::GraphRunner runner;
    std::string err;
    REQUIRE_MESSAGE(runner.build(d, &err), err);
    AVFrame* f = av_frame_alloc();
    TimeUs pts = 0;
    int r;
    while ((r = runner.pullVideo(f, &pts)) > 0) {
        std::uint64_t suma = 0;
        for (int y = 0; y < f->height; ++y)
            for (int x = 0; x < f->width; ++x) suma += f->data[0][y * f->linesize[0] + x];
        sumas.push_back(suma);
        if (ancho) *ancho = f->width;
        av_frame_unref(f);
    }
    CHECK_MESSAGE(r == 0, runner.ultimoError());
    av_frame_free(&f);
    return sumas;
}

} // namespace

TEST_CASE("GraphRunner: un PiP que empieza en 3 s no congela el fondo") {
    controller::EditorController ctrl;
    REQUIRE(ctrl.addTrack(TrackKind::Video));  // (queda seleccionada la pista nueva)
    ctrl.seleccionarPista(ctrl.project().timeline().firstTrack(TrackKind::Video)->id);
    const auto imp = ctrl.importFiles({medioPrueba("video_audio.mp4")});  // V1 0–5
    REQUIRE(imp.clips.size() == 1);
    const Track* v2 = nullptr;
    for (const Track& t : ctrl.project().timeline().tracks)
        if (t.kind == TrackKind::Video && t.id != ctrl.project().timeline().firstTrack(TrackKind::Video)->id) v2 = &t;
    REQUIRE(v2);
    ctrl.seleccionarPista(v2->id);
    const auto pip = ctrl.importFiles({medioPrueba("solo_video.mp4")});  // V2 0–3
    REQUIRE(pip.clips.size() == 1);
    REQUIRE(ctrl.moveClip(pip.clips[0], v2->id, s(3)));  // V2 3–6
    REQUIRE(ctrl.setPipTransform(pip.clips[0], {0.65, 0.65, 0.3, 1}));

    const engine::RenderSettings rs{320, 240, {25, 1}};
    int ancho = 0;
    const auto sumas = sumasDeCuadros(FilterGraphBuilder::build(snapshot(ctrl.project()), rs, 0), &ancho);
    CHECK(ancho == 320);
    REQUIRE(sumas.size() == 150);  // 6 s a 25 fps
    // Antes de los 3 s el fondo se mueve (testsrc2 cambia en cada cuadro).
    CHECK(sumas[10] != sumas[20]);
    CHECK(sumas[40] != sumas[60]);

    // Con el PiP, los cuadros de 3 a 5 s difieren de los del video solo.
    controller::EditorController solo;
    REQUIRE(solo.importFiles({medioPrueba("video_audio.mp4")}).clips.size() == 1);
    const auto sinPip = sumasDeCuadros(FilterGraphBuilder::build(snapshot(solo.project()), rs, 0));
    REQUIRE(sinPip.size() == 125);
    CHECK(sumas[50] == sinPip[50]);   // a los 2 s, igual
    CHECK(sumas[100] != sinPip[100]); // a los 4 s, con el PiP encima
}

TEST_CASE("GraphRunner: PiP parcialmente fuera y semitransparente") {
    controller::EditorController ctrl;
    Id v2 = kSinId;
    REQUIRE(ctrl.addTrack(TrackKind::Video));
    for (const Track& t : ctrl.project().timeline().tracks)
        if (t.kind == TrackKind::Video && t.id != ctrl.project().timeline().firstTrack(TrackKind::Video)->id) v2 = t.id;
    ctrl.seleccionarPista(ctrl.project().timeline().firstTrack(TrackKind::Video)->id);
    REQUIRE(ctrl.importFiles({medioPrueba("video_audio.mp4")}).clips.size() == 1);
    ctrl.seleccionarPista(v2);
    const auto pip = ctrl.importFiles({medioPrueba("solo_video.mp4")});
    REQUIRE(pip.clips.size() == 1);
    REQUIRE(ctrl.setPipTransform(pip.clips[0], {0.9, 0.0, 0.5, 0.5}));
    const auto sumas = sumasDeCuadros(FilterGraphBuilder::build(snapshot(ctrl.project()), {320, 240, {25, 1}}, 0));
    CHECK(sumas.size() == 125);
}

TEST_CASE("GraphRunner: un clip en V2 por defecto mide lo mismo que en V1") {
    // Lienzo cuadrado: el video queda con bandas, y deben coincidir en las dos pistas.
    const engine::RenderSettings rs{320, 320, {25, 1}};
    controller::EditorController enV1;
    REQUIRE(enV1.importFiles({medioPrueba("solo_video.mp4")}).clips.size() == 1);
    const auto sumasV1 = sumasDeCuadros(FilterGraphBuilder::build(snapshot(enV1.project()), rs, 0));

    controller::EditorController enV2;
    REQUIRE(enV2.addTrack(TrackKind::Video));
    for (const Track& t : enV2.project().timeline().tracks)
        if (t.kind == TrackKind::Video && t.id != enV2.project().timeline().firstTrack(TrackKind::Video)->id)
            enV2.seleccionarPista(t.id);
    REQUIRE(enV2.importFiles({medioPrueba("solo_video.mp4")}).clips.size() == 1);
    const auto sumasV2 = sumasDeCuadros(FilterGraphBuilder::build(snapshot(enV2.project()), rs, 0));

    REQUIRE(sumasV1.size() == sumasV2.size());
    REQUIRE(sumasV1.size() > 10);
    CHECK(static_cast<double>(sumasV2[10]) == doctest::Approx(static_cast<double>(sumasV1[10])).epsilon(0.02));
}

TEST_CASE("GraphRunner: transición entre un clip de V1 reducido y otro a pantalla completa") {
    controller::EditorController ctrl;
    const auto imp = ctrl.importFiles({medioPrueba("solo_video.mp4"), medioPrueba("solo_video.mp4")});  // 0–3, 3–6
    REQUIRE(imp.clips.size() == 2);
    REQUIRE(ctrl.setPipTransform(imp.clips[0], {0.25, 0.25, 0.5, 1}));
    REQUIRE(ctrl.addTransition(imp.clips[0], TransitionStyle::Dissolve, s(1)));
    const auto sumas = sumasDeCuadros(FilterGraphBuilder::build(snapshot(ctrl.project()), {320, 240, {25, 1}}, 0));
    CHECK(sumas.size() == 125);  // 3 + 3 - 1 s

    // El clip reducido deja negro alrededor: su cuadro es más oscuro que a pantalla completa.
    controller::EditorController completo;
    REQUIRE(completo.importFiles({medioPrueba("solo_video.mp4")}).clips.size() == 1);
    const auto sumasCompleto = sumasDeCuadros(FilterGraphBuilder::build(snapshot(completo.project()), {320, 240, {25, 1}}, 0));
    REQUIRE(sumasCompleto.size() > 10);
    CHECK(sumas[10] < sumasCompleto[10]);
}
