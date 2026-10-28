// TestController.cpp
// Escenarios que pasan por el controller (usan medios de prueba reales).
#include <doctest/doctest.h>

#include "EditorController/EditorController.h"
#include "MediosPrueba.h"

using namespace model;

TEST_CASE("Importación múltiple con un archivo inválido") {
    controller::EditorController ctrl;
    const auto r = ctrl.importFiles({medioPrueba("video_audio.mp4"), medioPrueba("falso.mp4"),
                                     medioPrueba("solo_video.mp4")});
    CHECK(r.clips.size() == 2);
    REQUIRE(r.errores.size() == 1);
    CHECK(r.errores[0].find("falso.mp4") != std::string::npos);

    // Los dos válidos quedan en V1, en orden y sin hueco.
    const Track* v1 = ctrl.project().timeline().firstTrack(TrackKind::Video);
    REQUIRE(v1->clips.size() == 2);
    CHECK(v1->clips[0].timelineStart == 0);
    CHECK(v1->clips[1].timelineStart == v1->clips[0].timelineEnd());
    // El lienzo lo define el primer video (320x240 a 25 fps).
    CHECK(ctrl.project().canvas().width == 320);
    CHECK(ctrl.project().canvas().height == 240);
}

TEST_CASE("Los archivos de solo audio van a una pista de audio") {
    controller::EditorController ctrl;
    const auto r = ctrl.importFiles({medioPrueba("tono.mp3")});
    REQUIRE(r.errores.empty());
    const Track* pista = nullptr;
    REQUIRE(ctrl.project().timeline().findClip(r.clips.at(0), &pista));
    CHECK(pista->kind == TrackKind::Audio);
}

TEST_CASE("Importar en la pista de video seleccionada") {
    controller::EditorController ctrl;
    REQUIRE(ctrl.addTrack(TrackKind::Video));  // crea V2 y la selecciona
    const Id v2 = ctrl.seleccion().trackId;
    const auto r = ctrl.importFiles({medioPrueba("solo_video.mp4")});
    const Track* pista = nullptr;
    REQUIRE(ctrl.project().timeline().findClip(r.clips.at(0), &pista));
    CHECK(pista->id == v2);
}

TEST_CASE("La selección se corrige al borrar la pista seleccionada") {
    controller::EditorController ctrl;
    ctrl.addTrack(TrackKind::Video);
    const Id v2 = ctrl.seleccion().trackId;
    REQUIRE(ctrl.removeTrack(v2));
    CHECK(ctrl.seleccion().trackId == ctrl.project().timeline().firstTrack(TrackKind::Video)->id);
}
