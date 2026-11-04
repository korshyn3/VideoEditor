// TestTiempo.cpp
// Capa: TESTS. Utilidades de tiempo del modelo (EditorModel/Tiempo).
#include <doctest/doctest.h>

#include "EditorModel/Tiempo.h"

using namespace model;

TEST_CASE("Conversión segundos <-> TimeUs") {
    CHECK(segundosATiempo(1.5) == 1'500'000);
    CHECK(tiempoASegundos(2'250'000) == doctest::Approx(2.25));
}

TEST_CASE("formatearTiempo") {
    CHECK(formatearTiempo(0) == "0:00.000");
    CHECK(formatearTiempo(segundosATiempo(65.25)) == "1:05.250");
    CHECK(formatearTiempo(segundosATiempo(3725.0)) == "1:02:05.000");
}
