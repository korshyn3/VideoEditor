// TestEscalaTiempo.cpp
// Conversión tiempo <-> píxel del timeline (src/VideoEditor/EscalaTiempo, sin MFC).
#include <doctest/doctest.h>

#include "VideoEditor/EscalaTiempo.h"

using model::segundosATiempo;

TEST_CASE("EscalaTiempo: conversión ida y vuelta") {
    EscalaTiempo e;
    e.setOrigenX(80);
    e.setPxPorSegundo(50);
    CHECK(e.tiempoAX(0) == 80);
    CHECK(e.tiempoAX(segundosATiempo(2)) == 180);
    CHECK(e.xATiempo(180) == segundosATiempo(2));

    e.setScrollPx(100);
    CHECK(e.tiempoAX(segundosATiempo(2)) == 80);
    CHECK(e.xATiempo(80) == segundosATiempo(2));
}

TEST_CASE("EscalaTiempo: el zoom mantiene fijo el instante bajo el mouse") {
    EscalaTiempo e;
    e.setOrigenX(80);
    e.setPxPorSegundo(50);
    const int x = 330;  // 5 s
    const auto antes = e.xATiempo(x);
    e.zoomAlrededorDe(x, 2.0);
    CHECK(e.pxPorSegundo() == doctest::Approx(100.0));
    CHECK(e.xATiempo(x) == antes);
}

TEST_CASE("EscalaTiempo: límites del zoom y del scroll") {
    EscalaTiempo e;
    e.setPxPorSegundo(1e9);
    CHECK(e.pxPorSegundo() == doctest::Approx(EscalaTiempo::kZoomMaximo));
    e.setPxPorSegundo(0);
    CHECK(e.pxPorSegundo() == doctest::Approx(EscalaTiempo::kZoomMinimo));
    e.setScrollPx(-10);
    CHECK(e.scrollPx() == 0);
}
