// TestEscalaTiempo.cpp
// Capa: TESTS.
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

namespace {
constexpr model::Id kV1 = 1;
constexpr model::Id kV2 = 2;
TramoIman tramo(model::Id pista, double inicio, double fin) {
    return {pista, segundosATiempo(inicio), segundosATiempo(fin)};
}
}  // namespace

TEST_CASE("Pegado: el umbral depende del zoom") {
    // A termina en 5 s; B (5 s) se suelta en 5.1 s: 0.1 s de separación.
    const std::vector<TramoIman> fijos{tramo(kV1, 0, 5)};
    const std::vector<TramoIman> moviles{tramo(kV1, 8, 13)};
    const model::TimeUs delta = segundosATiempo(5.1) - segundosATiempo(8);

    EscalaTiempo e;
    e.setPxPorSegundo(50);  // 0.1 s = 5 px: se pega
    auto p = buscarPegado(moviles, fijos, delta, e.umbralIman());
    REQUIRE(p);
    CHECK(p->delta == segundosATiempo(5) - segundosATiempo(8));
    CHECK(p->punto == segundosATiempo(5));

    e.setPxPorSegundo(200);  // 0.1 s = 20 px: no se pega
    CHECK_FALSE(buscarPegado(moviles, fijos, delta, e.umbralIman()));
}

TEST_CASE("Pegado: fin contra inicio del siguiente y 0 del timeline") {
    const model::TimeUs umbral = segundosATiempo(0.2);
    const std::vector<TramoIman> moviles{tramo(kV1, 2, 4)};

    auto p = buscarPegado(moviles, {tramo(kV1, 10, 12)}, segundosATiempo(5.9), umbral);
    REQUIRE(p);
    CHECK(p->delta == segundosATiempo(6));
    CHECK(p->punto == segundosATiempo(10));

    p = buscarPegado(moviles, {}, segundosATiempo(-1.9), umbral);
    REQUIRE(p);
    CHECK(p->delta == segundosATiempo(-2));
    CHECK(p->punto == 0);
}

TEST_CASE("Pegado: gana el más cercano de la pista") {
    const model::TimeUs umbral = segundosATiempo(0.5);
    const std::vector<TramoIman> moviles{tramo(kV1, 20, 22)};
    const model::TimeUs delta = segundosATiempo(-14.9);  // queda en 5.1 s

    auto p = buscarPegado(moviles, {tramo(kV1, 0, 4.8), tramo(kV1, 4.8, 5)}, delta, umbral);
    REQUIRE(p);
    CHECK(p->punto == segundosATiempo(5));
}

TEST_CASE("Pegado: se alinea con otras pistas por cualquier borde") {
    const model::TimeUs umbral = segundosATiempo(0.2);
    const std::vector<TramoIman> musica{tramo(kV2, 20, 30)};

    // Inicio contra inicio: en la misma pista quedaría encima, en otra pista sirve.
    auto p = buscarPegado(musica, {tramo(kV1, 5, 9)}, segundosATiempo(-14.9), umbral);
    REQUIRE(p);
    CHECK(p->delta == segundosATiempo(-15));
    CHECK(p->punto == segundosATiempo(5));

    // Fin contra fin.
    p = buscarPegado(musica, {tramo(kV1, 5, 25)}, segundosATiempo(-4.9), umbral);
    REQUIRE(p);
    CHECK(p->delta == segundosATiempo(-5));
    CHECK(p->punto == segundosATiempo(25));

    // En la misma pista, inicio contra inicio no se ofrece.
    CHECK_FALSE(buscarPegado(musica, {tramo(kV2, 5, 9)}, segundosATiempo(-14.9), umbral));
}

TEST_CASE("Pegado: la pista propia tiene prioridad sobre las otras") {
    const model::TimeUs umbral = segundosATiempo(0.5);
    // B queda en 5.3 s. El borde de V2 en 5.2 s está más cerca, pero gana el de su pista,
    // y por eso B no termina encima de A.
    const std::vector<TramoIman> fijos{tramo(kV1, 0, 5), tramo(kV2, 5.2, 8)};
    auto p = buscarPegado({tramo(kV1, 20, 22)}, fijos, segundosATiempo(-14.7), umbral);
    REQUIRE(p);
    CHECK(p->punto == segundosATiempo(5));
}

TEST_CASE("Pegado: un borde recortado se trata como un tramo sin ancho") {
    const model::TimeUs umbral = segundosATiempo(0.2);
    const model::TimeUs borde = segundosATiempo(10);
    const std::vector<TramoIman> movil{{kV1, borde, borde}};

    // Vecino de su pista.
    auto p = buscarPegado(movil, {tramo(kV1, 12, 14)}, segundosATiempo(1.9), umbral);
    REQUIRE(p);
    CHECK(p->delta == segundosATiempo(2));

    // Fin de un clip de otra pista.
    p = buscarPegado(movil, {tramo(kV2, 3, 11)}, segundosATiempo(0.9), umbral);
    REQUIRE(p);
    CHECK(p->delta == segundosATiempo(1));
}

TEST_CASE("Pegado: un grupo se pega por cualquiera de sus clips y no pasa de 0") {
    const model::TimeUs umbral = segundosATiempo(0.5);
    // El clip de V2 se pega al fijo de V2 y el de V1 se corre lo mismo.
    const std::vector<TramoIman> grupo{tramo(kV1, 10, 12), tramo(kV2, 11, 13)};
    auto p = buscarPegado(grupo, {tramo(kV2, 0, 8)}, segundosATiempo(-2.9), umbral);
    REQUIRE(p);
    CHECK(p->delta == segundosATiempo(-3));

    // Pegar el fin al inicio de un fijo dejaría el clip antes de 0: no se ofrece.
    CHECK_FALSE(buscarPegado({tramo(kV1, 5, 10)}, {tramo(kV1, 2, 3)}, segundosATiempo(-8), umbral));
}
