// TestEfectos.cpp
// Capa: TESTS.
// Escenarios de specs/video-effects (fase 6): lista de efectos por clip, límites de los
// parámetros, fades, efectos al dividir/recortar, su traducción a filtros y el render.
#include <doctest/doctest.h>

#include "EditorController/EditorController.h"
#include "EditorEngine/FilterGraphBuilder.h"
#include "EditorEngine/GraphRunner.h"
#include "EditorModel/Project.h"
#include "MediosPrueba.h"

#include <cmath>

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
    m.hasAudio = true;
    m.sampleRate = 48000;
    m.channels = 2;
    return m;
}

// Proyecto con un clip de video de `segundos` en V1.
struct UnClip {
    Project p;
    Id v1 = kSinId, clip = kSinId;

    explicit UnClip(double segundos = 10) {
        v1 = p.timeline().firstTrack(TrackKind::Video)->id;
        p.appendMedia(v1, p.addMedia(videoFalso("A.mp4", segundos)), &clip);
    }
    const Clip& c(Id id) const { return *p.timeline().findClip(id); }
    const std::vector<Effect>& efectos() const { return c(clip).effects; }
};

bool contiene(const std::string& txt, const std::string& sub) { return txt.find(sub) != std::string::npos; }

engine::TimelineSnapshot snapshot(const Project& p) { return {p.timeline(), p.media(), p.canvas()}; }

} // namespace

// ---------------------------------------------------------------------------
// Modelo
// ---------------------------------------------------------------------------

TEST_CASE("Agregar, reordenar y quitar efectos") {
    UnClip u;
    REQUIRE(u.p.addEffect(u.clip, Grayscale{}));
    REQUIRE(u.p.addEffect(u.clip, Blur{10}));
    REQUIRE(u.efectos().size() == 2);
    CHECK(std::holds_alternative<Grayscale>(u.efectos()[0]));

    REQUIRE(u.p.moveEffect(u.clip, 1, -1));  // el desenfoque pasa primero
    CHECK(std::holds_alternative<Blur>(u.efectos()[0]));
    CHECK_FALSE(u.p.moveEffect(u.clip, 0, -1));  // ya es el primero
    CHECK_FALSE(u.p.moveEffect(u.clip, 1, +1));  // ya es el último

    REQUIRE(u.p.removeEffect(u.clip, 0));
    REQUIRE(u.efectos().size() == 1);
    CHECK(std::holds_alternative<Grayscale>(u.efectos()[0]));
    CHECK_FALSE(u.p.removeEffect(u.clip, 5));
}

TEST_CASE("Los efectos solo se aplican a clips de video") {
    UnClip u;
    const Id audio = u.c(u.clip).linkedClipId;
    REQUIRE(audio != kSinId);
    CHECK_FALSE(u.p.addEffect(audio, Grayscale{}));
    CHECK(u.c(audio).effects.empty());
}

TEST_CASE("Valor fuera de rango") {
    UnClip u;
    REQUIRE(u.p.addEffect(u.clip, BrightnessContrast{0.0, 5.0}));  // contraste = 5
    CHECK(std::get<BrightnessContrast>(u.efectos()[0]).contrast == 3.0);
    REQUIRE(u.p.setEffect(u.clip, 0, BrightnessContrast{-2.0, -1.0}));
    CHECK(std::get<BrightnessContrast>(u.efectos()[0]).brightness == -1.0);
    CHECK(std::get<BrightnessContrast>(u.efectos()[0]).contrast == 0.0);

    REQUIRE(u.p.addEffect(u.clip, Blur{80}));
    CHECK(std::get<Blur>(u.efectos()[1]).radius == 50.0);
    // Cambiar un efecto por otro de distinto tipo no está permitido.
    CHECK_FALSE(u.p.setEffect(u.clip, 1, Grayscale{}));
}

TEST_CASE("Fade más largo que el clip") {
    UnClip u(6);
    const auto rev = u.p.revision();
    const OpResult r = u.p.addEffect(u.clip, Fade{s(4), s(4)});
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.error.empty());  // se informa al usuario
    CHECK(u.efectos().empty());
    CHECK(u.p.revision() == rev);

    REQUIRE(u.p.addEffect(u.clip, Fade{s(3), s(3)}));  // justo la duración: se acepta
    CHECK_FALSE(u.p.setEffect(u.clip, 0, Fade{s(3), s(3.5)}));
    CHECK(std::get<Fade>(u.efectos()[0]).fadeOut == s(3));
}

TEST_CASE("Split con efectos") {
    UnClip u(10);
    REQUIRE(u.p.addEffect(u.clip, Grayscale{}));
    REQUIRE(u.p.addEffect(u.clip, Fade{s(1), s(2)}));
    Id derecho = kSinId;
    REQUIRE(u.p.split(u.clip, s(5), &derecho));

    const auto& izq = u.c(u.clip).effects;
    const auto& der = u.c(derecho).effects;
    REQUIRE(izq.size() == 2);
    REQUIRE(der.size() == 2);
    CHECK(std::holds_alternative<Grayscale>(izq[0]));
    CHECK(std::holds_alternative<Grayscale>(der[0]));
    CHECK(std::get<Fade>(izq[1]).fadeIn == s(1));
    CHECK(std::get<Fade>(izq[1]).fadeOut == 0);
    CHECK(std::get<Fade>(der[1]).fadeIn == 0);
    CHECK(std::get<Fade>(der[1]).fadeOut == s(2));
}

TEST_CASE("Split de un clip con solo fade in deja la segunda mitad sin fade") {
    UnClip u(10);
    REQUIRE(u.p.addEffect(u.clip, Fade{s(1), 0}));
    Id derecho = kSinId;
    REQUIRE(u.p.split(u.clip, s(5), &derecho));
    CHECK(u.c(u.clip).effects.size() == 1);
    CHECK(u.c(derecho).effects.empty());  // un fade sin fade in ni fade out se quita
}

TEST_CASE("Recortar un clip ajusta sus fades") {
    UnClip u(10);
    REQUIRE(u.p.addEffect(u.clip, Fade{s(4), s(4)}));
    REQUIRE(u.p.trimClip(u.clip, 0, s(6)));  // queda de 6 s
    const Fade f = std::get<Fade>(u.efectos()[0]);
    CHECK(f.fadeIn == s(4));
    CHECK(f.fadeOut == s(2));
    CHECK(f.fadeIn + f.fadeOut <= u.c(u.clip).duration());
}

TEST_CASE("Mover un clip conserva sus efectos") {
    UnClip u(10);
    REQUIRE(u.p.addEffect(u.clip, Blur{3}));
    REQUIRE(u.p.moveClip(u.clip, u.v1, s(20)));
    REQUIRE(u.efectos().size() == 1);
    CHECK(std::get<Blur>(u.efectos()[0]).radius == 3.0);
}

TEST_CASE("Controller: agregar un fade con sus valores por defecto") {
    controller::EditorController ctrl;
    const auto imp = ctrl.importFiles({medioPrueba("solo_video.mp4")});
    REQUIRE(imp.errores.empty());
    REQUIRE(imp.clips.size() == 1);
    const Id clip = imp.clips[0];
    REQUIRE(ctrl.addEffect(clip, 3));  // Fade: 1 s de fade in por defecto
    const Fade f = std::get<Fade>(ctrl.project().timeline().findClip(clip)->effects[0]);
    CHECK(f.fadeIn == s(1));
    CHECK(f.fadeOut == 0);
}

// ---------------------------------------------------------------------------
// Cadena de filtros
// ---------------------------------------------------------------------------

TEST_CASE("FilterGraphBuilder: efectos en el orden de la lista") {
    UnClip u(10);
    REQUIRE(u.p.addEffect(u.clip, Grayscale{}));
    REQUIRE(u.p.addEffect(u.clip, BrightnessContrast{0.5, 2.0}));
    REQUIRE(u.p.addEffect(u.clip, Blur{10}));
    REQUIRE(u.p.addEffect(u.clip, Fade{s(1), s(2)}));
    const auto d = FilterGraphBuilder::build(snapshot(u.p), {640, 360, {25, 1}}, 0);
    const std::string& g = d.filtros;

    const auto hue = g.find("hue=s=0");
    // K = 128 + 0.5·255 - 128·2 = -0.5
    const auto lut = g.find("lutyuv=y='clip(val*2.0000+-0.5000,0,255)'");
    const auto blur = g.find("gblur=sigma=10.0000");
    const auto fin = g.find("fade=t=in:st=0:d=1.000000");
    const auto fout = g.find("fade=t=out:st=8.000000:d=2.000000");
    REQUIRE(hue != std::string::npos);
    REQUIRE(lut != std::string::npos);
    REQUIRE(blur != std::string::npos);
    REQUIRE(fin != std::string::npos);
    REQUIRE(fout != std::string::npos);
    CHECK(hue < lut);
    CHECK(lut < blur);
    CHECK(blur < fin);
    // Los efectos van después de normalizar el formato y antes de fijar los cuadros.
    CHECK(g.find("format=yuv420p,hue=s=0") != std::string::npos);
    CHECK(fout < g.find("tpad=stop_mode=clone"));
}

TEST_CASE("FilterGraphBuilder: desenfoque 0 no agrega filtro") {
    UnClip u(10);
    REQUIRE(u.p.addEffect(u.clip, Blur{0}));
    const auto d = FilterGraphBuilder::build(snapshot(u.p), {640, 360, {25, 1}}, 0);
    CHECK_FALSE(contiene(d.filtros, "gblur"));
}

TEST_CASE("FilterGraphBuilder: el fade usa el tiempo del clip al arrancar a la mitad") {
    UnClip u(10);
    REQUIRE(u.p.addEffect(u.clip, Fade{s(4), 0}));
    const auto d = FilterGraphBuilder::build(snapshot(u.p), {640, 360, {25, 1}}, s(2));
    // El segmento arranca 2 s dentro del clip: sus pts empiezan en 2 s para el fade.
    CHECK(contiene(d.filtros, "[in0]setpts=PTS-STARTPTS+2.000000/TB,"));
    CHECK(contiene(d.filtros, "fade=t=in:st=0:d=4.000000"));
}

TEST_CASE("FilterGraphBuilder: solo el clip con efectos los recibe") {
    Project p;
    const Id v1 = p.timeline().firstTrack(TrackKind::Video)->id;
    Id a = kSinId, b = kSinId;
    p.appendMedia(v1, p.addMedia(videoFalso("A.mp4", 5)), &a);
    p.appendMedia(v1, p.addMedia(videoFalso("B.mp4", 5)), &b);
    REQUIRE(p.addEffect(b, Grayscale{}));
    const auto d = FilterGraphBuilder::build(snapshot(p), {640, 360, {25, 1}}, 0);
    const auto pos = d.filtros.find("hue=s=0");
    REQUIRE(pos != std::string::npos);
    CHECK(d.filtros.find("hue=s=0", pos + 1) == std::string::npos);  // una sola vez
    CHECK(pos > d.filtros.find("[in1]"));                             // en el segmento de B
}

// ---------------------------------------------------------------------------
// Render real
// ---------------------------------------------------------------------------

namespace {

// Promedios de Y, U y V del primer cuadro (yuv420p) que produce el grafo, y cuánto se
// alejan U y V de 128 (sin color) en promedio por píxel.
struct Promedios {
    double y = 0, u = 0, v = 0;
    double desvioU = 0, desvioV = 0;
};

Promedios primerCuadro(const engine::GraphDescription& d) {
    engine::GraphRunner runner;
    std::string err;
    REQUIRE_MESSAGE(runner.build(d, &err), err);
    AVFrame* f = av_frame_alloc();
    TimeUs pts = 0;
    REQUIRE_MESSAGE(runner.pullVideo(f, &pts) > 0, runner.ultimoError());
    REQUIRE(f->format == AV_PIX_FMT_YUV420P);
    auto promedio = [](const uint8_t* datos, int linesize, int ancho, int alto, double* desvio = nullptr) {
        double suma = 0, sumaDesvio = 0;
        for (int y = 0; y < alto; ++y) {
            for (int x = 0; x < ancho; ++x) {
                suma += datos[y * linesize + x];
                sumaDesvio += std::abs(datos[y * linesize + x] - 128.0);
            }
        }
        const double n = static_cast<double>(ancho) * alto;
        if (desvio) *desvio = sumaDesvio / n;
        return suma / n;
    };
    Promedios p;
    p.y = promedio(f->data[0], f->linesize[0], f->width, f->height);
    p.u = promedio(f->data[1], f->linesize[1], f->width / 2, f->height / 2, &p.desvioU);
    p.v = promedio(f->data[2], f->linesize[2], f->width / 2, f->height / 2, &p.desvioV);
    av_frame_free(&f);
    return p;
}

} // namespace

TEST_CASE("GraphRunner: escala de grises deja U = V = 128") {
    controller::EditorController ctrl;
    const auto imp = ctrl.importFiles({medioPrueba("video_audio.mp4")});
    REQUIRE(imp.clips.size() == 1);
    const Id clip = imp.clips[0];
    const engine::RenderSettings rs{320, 240, {25, 1}};
    const engine::TimelineSnapshot sinEfecto{ctrl.project().timeline(), ctrl.project().media(), ctrl.project().canvas()};
    const Promedios color = primerCuadro(FilterGraphBuilder::build(sinEfecto, rs, s(1), s(1.04)));

    REQUIRE(ctrl.addEffect(clip, 0));  // escala de grises
    const engine::TimelineSnapshot snap{ctrl.project().timeline(), ctrl.project().media(), ctrl.project().canvas()};
    const Promedios gris = primerCuadro(FilterGraphBuilder::build(snap, rs, s(1), s(1.04)));
    CHECK(gris.u == doctest::Approx(128).epsilon(0.01));
    CHECK(gris.v == doctest::Approx(128).epsilon(0.01));
    CHECK(gris.desvioU < 1.5);  // cada píxel, no solo el promedio
    CHECK(gris.desvioV < 1.5);
    // El original sí tiene color (si no, el test no probaría nada).
    CHECK(color.desvioU + color.desvioV > 10);
}

TEST_CASE("GraphRunner: fade in arranca en negro y brillo aclara la imagen") {
    controller::EditorController ctrl;
    const auto imp = ctrl.importFiles({medioPrueba("video_audio.mp4")});
    REQUIRE(imp.clips.size() == 1);
    const Id clip = imp.clips[0];
    const engine::RenderSettings rs{320, 240, {25, 1}};
    auto cuadroEn = [&](double t) {
        const engine::TimelineSnapshot snap{ctrl.project().timeline(), ctrl.project().media(), ctrl.project().canvas()};
        return primerCuadro(FilterGraphBuilder::build(snap, rs, s(t), s(t) + s(0.04)));
    };
    const Promedios normal = cuadroEn(2);

    REQUIRE(ctrl.addEffect(clip, 3));  // fade in de 1 s
    CHECK(cuadroEn(0).y < 20);         // negro (Y = 16 en rango limitado)
    const double medio = cuadroEn(0.5).y;  // a la mitad del fade (también tras un seek)
    CHECK(medio > 20);
    CHECK(medio < normal.y - 10);
    CHECK(cuadroEn(2).y == doctest::Approx(normal.y).epsilon(0.02));  // ya terminó

    REQUIRE(ctrl.removeEffect(clip, 0));
    REQUIRE(ctrl.addEffect(clip, 1));
    REQUIRE(ctrl.setEffect(clip, 0, BrightnessContrast{0.3, 1.0}));
    CHECK(cuadroEn(2).y > normal.y + 30);
}
