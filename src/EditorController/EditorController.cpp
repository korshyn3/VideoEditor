// EditorController.cpp
// Capa: CONTROLLER. Ver EditorController.h.
#include "EditorController/EditorController.h"

#include <algorithm>

namespace controller {

EditorController::EditorController() {
    // Por defecto se importa en V1.
    if (const model::Track* v1 = project_.timeline().firstTrack(model::TrackKind::Video))
        seleccion_.trackId = v1->id;
}

engine::ProbeResult EditorController::analizarArchivo(const std::string& rutaUtf8) const {
    return engine::MediaProbe::probe(rutaUtf8);
}

model::Id EditorController::pistaDestino(model::TrackKind kind) {
    // 1) la pista seleccionada, si es del tipo correcto;
    const model::Track* sel = project_.timeline().findTrack(seleccion_.trackId);
    if (sel && sel->kind == kind) return sel->id;
    // 2) la primera pista de ese tipo;
    if (const model::Track* primera = project_.timeline().firstTrack(kind)) return primera->id;
    // 3) si no hay ninguna (solo puede pasar con audio), se crea.
    model::Id nueva = model::kSinId;
    project_.addTrack(kind, &nueva);
    return nueva;
}

ImportResult EditorController::importFiles(const std::vector<std::string>& rutasUtf8) {
    ImportResult resultado;
    for (const std::string& ruta : rutasUtf8) {
        engine::ProbeResult probe = engine::MediaProbe::probe(ruta);
        if (!probe.ok) {
            resultado.errores.push_back(probe.error);
            continue;
        }
        const model::TrackKind kind = probe.info.hasVideo ? model::TrackKind::Video : model::TrackKind::Audio;
        const model::Id mediaId = project_.addMedia(std::move(probe.info));
        model::Id clipId = model::kSinId;
        const model::OpResult r = project_.appendMedia(pistaDestino(kind), mediaId, &clipId);
        if (r.ok)
            resultado.clips.push_back(clipId);
        else
            resultado.errores.push_back(r.error);
    }
    return resultado;
}

model::OpResult EditorController::addTrack(model::TrackKind kind) {
    model::Id nueva = model::kSinId;
    model::OpResult r = project_.addTrack(kind, &nueva);
    if (r.ok) seleccionarPista(nueva);
    return r;
}

model::OpResult EditorController::removeTrack(model::Id trackId) {
    model::OpResult r = project_.removeTrack(trackId);
    validarSeleccion();
    return r;
}

model::OpResult EditorController::moveClip(model::Id clipId, model::Id destTrackId, model::TimeUs newStart) {
    model::OpResult r = project_.moveClip(clipId, destTrackId, newStart);
    if (r.ok) seleccionarClip(clipId);  // la pista seleccionada pasa a ser la del clip
    return r;
}

void EditorController::seleccionarPista(model::Id trackId) {
    if (project_.timeline().findTrack(trackId)) {
        seleccion_.trackId = trackId;
        seleccion_.clipId = model::kSinId;
    }
}

void EditorController::seleccionarClip(model::Id clipId) {
    const model::Track* pista = nullptr;
    if (project_.timeline().findClip(clipId, &pista)) {
        seleccion_.clipId = clipId;
        seleccion_.trackId = pista->id;
    }
}

void EditorController::limpiarSeleccionClip() {
    seleccion_.clipId = model::kSinId;
}

void EditorController::setPlayhead(model::TimeUs t) {
    playhead_ = std::max<model::TimeUs>(0, t);
}

void EditorController::validarSeleccion() {
    const model::Timeline& tl = project_.timeline();
    if (!tl.findClip(seleccion_.clipId)) seleccion_.clipId = model::kSinId;
    if (!tl.findTrack(seleccion_.trackId)) {
        const model::Track* v1 = tl.firstTrack(model::TrackKind::Video);
        seleccion_.trackId = v1 ? v1->id : model::kSinId;
    }
}

} // namespace controller
