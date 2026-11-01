// TestRender.cpp
// Fase 3: MediaDecoder, FilterGraphBuilder (cadenas) y GraphRunner (render real).
#include <doctest/doctest.h>

#include "EditorController/EditorController.h"
#include "EditorEngine/FilterGraphBuilder.h"
#include "EditorEngine/GraphRunner.h"
#include "EditorEngine/MediaDecoder.h"
#include "MediosPrueba.h"

using namespace model;
using engine::FilterGraphBuilder;

namespace {

MediaInfo medio(const std::string& ruta, double seg, bool audio) {
    MediaInfo m;
    m.path = ruta;
    m.duration = segundosATiempo(seg);
    m.hasVideo = true;
    m.width = 1280;
    m.height = 720;
    m.fps = {25, 1};
    m.hasAudio = audio;
    return m;
}

engine::TimelineSnapshot snapshot(const Project& p) {
    return {p.timeline(), p.media(), p.canvas()};
}

bool contiene(const std::string& s, const std::string& sub) {
    return s.find(sub) != std::string::npos;
}

// Cuenta cuadros de video y muestras de audio que produce el grafo.
struct Conteo {
    int cuadros = 0;
    long long muestras = 0;
    TimeUs primerPts = -1, ultimoPts = -1;
    int ancho = 0, alto = 0;
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
        c.ultimoPts = pts;
        c.ancho = f->width;
        c.alto = f->height;
        ++c.cuadros;
        av_frame_unref(f);
    }
    CHECK_MESSAGE(r == 0, runner.ultimoError());
    while ((r = runner.pullAudio(f, &pts)) > 0) {
        CHECK(f->sample_rate == 48000);
        CHECK(f->ch_layout.nb_channels == 2);
        CHECK(f->format == AV_SAMPLE_FMT_FLT);
        c.muestras += f->nb_samples;
        av_frame_unref(f);
    }
    CHECK_MESSAGE(r == 0, runner.ultimoError());
    av_frame_free(&f);
    return c;
}

} // namespace

// ---------------------------------------------------------------------------
// 3.1 MediaDecoder
// ---------------------------------------------------------------------------

TEST_CASE("MediaDecoder: seek preciso a 2.0 s") {
    engine::MediaDecoder dec;
    std::string err;
    REQUIRE_MESSAGE(dec.open(medioPrueba("video_audio.mp4"), engine::MediaDecoder::Tipo::Video, &err), err);
    REQUIRE(dec.setRange(segundosATiempo(2.0), segundosATiempo(5.0), &err));
    AVFrame* f = av_frame_alloc();
    REQUIRE(dec.nextFrame(f) == 1);
    const TimeUs pts = av_rescale_q(f->pts, dec.timeBase(), AVRational{1, 1'000'000});
    CHECK(pts >= segundosATiempo(2.0));
    CHECK(pts < segundosATiempo(2.0 + 1.0 / 25));

    // Hasta la salida (5 s) quedan 75 cuadros en total (25 fps).
    int cuadros = 1;
    while (dec.nextFrame(f) == 1) ++cuadros;
    CHECK(cuadros == 75);
    av_frame_free(&f);
}

TEST_CASE("MediaDecoder: audio con canales y rango") {
    engine::MediaDecoder dec;
    std::string err;
    REQUIRE(dec.open(medioPrueba("video_audio.mp4"), engine::MediaDecoder::Tipo::Audio, &err));
    REQUIRE(dec.setRange(segundosATiempo(1.0), segundosATiempo(2.0), &err));
    AVFrame* f = av_frame_alloc();
    REQUIRE(dec.nextFrame(f) == 1);
    CHECK(f->ch_layout.nb_channels == 1);
    CHECK(f->sample_rate == 48000);
    av_frame_free(&f);
}

TEST_CASE("MediaDecoder: archivo sin audio") {
    engine::MediaDecoder dec;
    std::string err;
    CHECK_FALSE(dec.open(medioPrueba("solo_video.mp4"), engine::MediaDecoder::Tipo::Audio, &err));
    CHECK(contiene(err, "no tiene audio"));
}

// ---------------------------------------------------------------------------
// 3.2 FilterGraphBuilder (sin FFmpeg: solo se compara el texto)
// ---------------------------------------------------------------------------

TEST_CASE("FilterGraphBuilder: dos clips seguidos, con y sin audio") {
    Project p;
    const Id v1 = p.timeline().firstTrack(TrackKind::Video)->id;
    p.appendMedia(v1, p.addMedia(medio("A.mp4", 10, true)));
    p.appendMedia(v1, p.addMedia(medio("B.mp4", 5, false)));
    const engine::RenderSettings rs{640, 360, {25, 1}};

    const auto d = FilterGraphBuilder::build(snapshot(p), rs, 0);
    REQUIRE_FALSE(d.vacio);
    CHECK(d.t0 == 0);
    CHECK(d.fin == segundosATiempo(15));
    // Entradas: video de A, video de B y audio de A (B no tiene audio).
    REQUIRE(d.inputs.size() == 3);
    CHECK(d.inputs[0].video);
    CHECK(d.inputs[0].mediaPath == "A.mp4");
    CHECK(d.inputs[1].mediaPath == "B.mp4");
    CHECK_FALSE(d.inputs[2].video);

    CHECK(contiene(d.filtros, "concat=n=2:v=1:a=0"));
    CHECK(contiene(d.filtros, "trim=end_frame=250"));  // A: 10 s * 25 fps
    CHECK(contiene(d.filtros, "trim=end_frame=125"));  // B: 5 s
    CHECK(contiene(d.filtros, "scale=640:360"));
    CHECK(contiene(d.filtros, "fps=25/1"));
    CHECK(contiene(d.filtros, "anullsrc"));                 // silencio durante B
    CHECK(contiene(d.filtros, "atrim=duration=5.000000"));
    CHECK(contiene(d.filtros, "concat=n=2:v=0:a=1"));
    CHECK(contiene(d.filtros, "[vout]"));
    CHECK(contiene(d.filtros, "[aout]"));
}

TEST_CASE("FilterGraphBuilder: arranque a mitad de un clip") {
    Project p;
    const Id v1 = p.timeline().firstTrack(TrackKind::Video)->id;
    p.appendMedia(v1, p.addMedia(medio("A.mp4", 10, true)));
    p.appendMedia(v1, p.addMedia(medio("B.mp4", 5, false)));

    const auto d = FilterGraphBuilder::build(snapshot(p), {640, 360, {25, 1}}, segundosATiempo(12.0));
    CHECK(d.t0 == segundosATiempo(12.0));
    REQUIRE(d.inputs.size() == 1);  // A ya terminó: solo el video de B
    CHECK(d.inputs[0].sourceIn == segundosATiempo(2.0));
    CHECK(contiene(d.filtros, "trim=end_frame=75"));  // quedan 3 s de B
    CHECK(contiene(d.filtros, "setpts=PTS-STARTPTS+12.000000/TB[vout]"));
}

TEST_CASE("FilterGraphBuilder: T0 se alinea a la rejilla de cuadros") {
    Project p;
    p.appendMedia(p.timeline().firstTrack(TrackKind::Video)->id, p.addMedia(medio("A.mp4", 10, false)));
    const auto d = FilterGraphBuilder::build(snapshot(p), {640, 360, {25, 1}}, segundosATiempo(1.03));
    CHECK(d.t0 == segundosATiempo(1.0));  // cuadros de 40 ms: 1.03 s cae en el cuadro 25
}

TEST_CASE("FilterGraphBuilder: hueco entre clips") {
    Project p;
    const Id v1 = p.timeline().firstTrack(TrackKind::Video)->id;
    Id b = kSinId;
    p.appendMedia(v1, p.addMedia(medio("A.mp4", 10, false)));
    p.appendMedia(v1, p.addMedia(medio("B.mp4", 5, false)), &b);
    REQUIRE(p.moveClip(b, v1, segundosATiempo(12)));

    const auto d = FilterGraphBuilder::build(snapshot(p), {640, 360, {25, 1}}, 0);
    CHECK(contiene(d.filtros, "color=c=black:s=640x360:r=25/1,format=yuv420p,setsar=1,trim=end_frame=50"));
    CHECK(contiene(d.filtros, "concat=n=3:v=1:a=0"));
}

TEST_CASE("FilterGraphBuilder: timeline vacío") {
    Project p;
    CHECK(FilterGraphBuilder::build(snapshot(p), {640, 360, {25, 1}}, 0).vacio);
}

TEST_CASE("FilterGraphBuilder: resolución de preview") {
    const auto r = FilterGraphBuilder::resolucionPreview({1920, 1080, {30, 1}}, 960);
    CHECK(r.width == 960);
    CHECK(r.height == 540);
    const auto chica = FilterGraphBuilder::resolucionPreview({321, 241, {25, 1}}, 960);
    CHECK(chica.width == 320);
    CHECK(chica.height == 240);
}

// ---------------------------------------------------------------------------
// 3.3 GraphRunner (render real con los medios de prueba)
// ---------------------------------------------------------------------------

TEST_CASE("GraphRunner: 2 s de un timeline de dos clips") {
    controller::EditorController ctrl;
    REQUIRE(ctrl.importFiles({medioPrueba("video_audio.mp4"), medioPrueba("solo_video.mp4")}).errores.empty());
    const engine::TimelineSnapshot snap{ctrl.project().timeline(), ctrl.project().media(), ctrl.project().canvas()};
    const engine::RenderSettings rs{320, 240, {25, 1}};

    const auto d = FilterGraphBuilder::build(snap, rs, 0, segundosATiempo(2.0));
    const Conteo c = renderizar(d);
    CHECK(c.cuadros == 50);            // 2 s a 25 fps
    CHECK(c.primerPts == 0);
    CHECK(c.ultimoPts == segundosATiempo(1.96));
    CHECK(c.ancho == 320);
    CHECK(c.alto == 240);
    CHECK(c.muestras == 96000);        // 2 s a 48 kHz
}

TEST_CASE("GraphRunner: arranque dentro del segundo clip") {
    controller::EditorController ctrl;
    REQUIRE(ctrl.importFiles({medioPrueba("video_audio.mp4"), medioPrueba("solo_video.mp4")}).errores.empty());
    const engine::TimelineSnapshot snap{ctrl.project().timeline(), ctrl.project().media(), ctrl.project().canvas()};

    // El segundo clip (640x360, sin audio) ocupa 5..8 s; se escala al lienzo 320x240.
    const auto d = FilterGraphBuilder::build(snap, {320, 240, {25, 1}}, segundosATiempo(6.0));
    const Conteo c = renderizar(d);
    CHECK(c.primerPts == segundosATiempo(6.0));
    CHECK(c.cuadros == 50);            // de 6 s a 8 s
    CHECK(c.ancho == 320);
    CHECK(c.muestras == 96000);        // silencio, pero con la duración correcta
}

TEST_CASE("GraphRunner: el timeline completo dura lo esperado") {
    controller::EditorController ctrl;
    REQUIRE(ctrl.importFiles({medioPrueba("video_audio.mp4"), medioPrueba("solo_video.mp4")}).errores.empty());
    const engine::TimelineSnapshot snap{ctrl.project().timeline(), ctrl.project().media(), ctrl.project().canvas()};
    const Conteo c = renderizar(FilterGraphBuilder::build(snap, {320, 240, {25, 1}}, 0));
    CHECK(c.cuadros == 200);           // 8 s a 25 fps
    CHECK(c.muestras == 8 * 48000);
}

// Herramienta (omitida por defecto): imprime el grafo del timeline de prueba para
// ejecutarlo a mano con ffmpeg.exe. Uso:
//   EditorTests.exe -tc="Imprimir grafo de ejemplo" --no-skip
TEST_CASE("Imprimir grafo de ejemplo" * doctest::skip()) {
    controller::EditorController ctrl;
    ctrl.importFiles({medioPrueba("video_audio.mp4"), medioPrueba("solo_video.mp4")});
    const engine::TimelineSnapshot snap{ctrl.project().timeline(), ctrl.project().media(), ctrl.project().canvas()};
    const auto d = FilterGraphBuilder::build(snap, {320, 240, {25, 1}}, 0);
    for (const auto& in : d.inputs)
        MESSAGE(in.label << (in.video ? " (video) " : " (audio) ") << in.mediaPath << " [" << in.sourceIn << ", "
                         << in.sourceOut << ") us");
    MESSAGE("\n" << d.filtros);
}

// ---------------------------------------------------------------------------
// 3.4 PlaybackEngine (hilo de render)
// ---------------------------------------------------------------------------

#include "EditorEngine/PlaybackEngine.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace {

// Espera (como máximo `ms`) a que el engine entregue un cuadro.
bool esperarCuadro(engine::PlaybackEngine& pe, engine::FrameBGRA& out, int ms = 5000) {
    for (int i = 0; i < ms / 10; ++i) {
        if (pe.tomarCuadro(out)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

} // namespace

TEST_CASE("PlaybackEngine: en pausa, un seek produce el cuadro de ese instante") {
    controller::EditorController ctrl;
    REQUIRE(ctrl.importFiles({medioPrueba("video_audio.mp4"), medioPrueba("solo_video.mp4")}).errores.empty());

    std::atomic<int> avisos{0};
    engine::PlaybackEngine pe([&] { ++avisos; });
    pe.setTimeline({ctrl.project().timeline(), ctrl.project().media(), ctrl.project().canvas()});
    pe.seek(segundosATiempo(6.5));

    engine::FrameBGRA c;
    REQUIRE(esperarCuadro(pe, c));
    CHECK(c.width == 320);  // lienzo 320x240 (< 960: no se reduce)
    CHECK(c.height == 240);
    CHECK(c.pixels.size() == 320u * 240u * 4u);
    CHECK(c.pts == segundosATiempo(6.48));  // cuadro de 40 ms que contiene 6.5 s
    // El aviso se envía justo DESPUÉS de dejar el cuadro en la cola: puede tardar un instante.
    for (int i = 0; i < 100 && avisos == 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(avisos > 0);
    CHECK_FALSE(pe.reproduciendo());
    CHECK(pe.error().empty());
}

TEST_CASE("PlaybackEngine: los seeks seguidos se agrupan (gana el último)") {
    controller::EditorController ctrl;
    REQUIRE(ctrl.importFiles({medioPrueba("video_audio.mp4")}).errores.empty());
    engine::PlaybackEngine pe;
    pe.setTimeline({ctrl.project().timeline(), ctrl.project().media(), ctrl.project().canvas()});
    for (int i = 0; i < 50; ++i) pe.seek(segundosATiempo(0.04 * i));
    pe.seek(segundosATiempo(4.0));

    engine::FrameBGRA c;
    // Se descartan cuadros intermedios hasta ver el del último seek.
    bool visto = false;
    for (int i = 0; i < 20 && !visto; ++i)
        if (esperarCuadro(pe, c)) visto = (c.pts == segundosATiempo(4.0));
    CHECK(visto);
}

TEST_CASE("PlaybackEngine: reproducir avanza el reloj y termina al final") {
    controller::EditorController ctrl;
    REQUIRE(ctrl.importFiles({medioPrueba("solo_video.mp4")}).errores.empty());  // 3 s
    engine::PlaybackEngine pe;
    pe.setTimeline({ctrl.project().timeline(), ctrl.project().media(), ctrl.project().canvas()});
    pe.seek(segundosATiempo(2.0));
    pe.play();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    CHECK(pe.reproduciendo());
    CHECK(pe.reloj() > segundosATiempo(2.0));

    // Al llegar al final (3 s) se detiene sola.
    for (int i = 0; i < 300 && pe.reproduciendo(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK_FALSE(pe.reproduciendo());
    CHECK(pe.reloj() == segundosATiempo(3.0));
}
