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

model::Id EditorController::pistaDestino(bool tieneVideo) {
    const model::Timeline& tl = project_.timeline();
    const model::Track* sel = tl.findTrack(seleccion_.trackId);

    // Un video va a una pista general seleccionada (solo su audio) o a una de video: la
    // seleccionada, la dueña de la pista de audio de video seleccionada, o V1.
    if (tieneVideo) {
        if (sel && sel->esAudioGeneral()) return sel->id;
        if (sel && sel->kind == model::TrackKind::Video) return sel->id;
        if (sel && sel->esAudioDeVideo() && tl.findTrack(sel->videoTrackId)) return sel->videoTrackId;
        return tl.firstTrack(model::TrackKind::Video)->id;
    }

    // Un archivo de audio va a la pista general seleccionada, o a la primera general (que
    // se crea si no existe). Nunca a una pista de audio de video.
    if (sel && sel->esAudioGeneral()) return sel->id;
    if (const model::Track* general = tl.firstGeneralAudioTrack()) return general->id;
    model::Id nueva = model::kSinId;
    project_.addTrack(model::TrackKind::Audio, &nueva);
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
        const bool tieneVideo = probe.info.hasVideo;
        const model::Id mediaId = project_.addMedia(std::move(probe.info));
        model::Id clipId = model::kSinId;
        const model::OpResult r = project_.appendMedia(pistaDestino(tieneVideo), mediaId, &clipId);
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

model::OpResult EditorController::moveClip(model::Id clipId, model::Id destTrackId, model::TimeUs newStart,
                                           bool sobrescribir) {
    model::OpResult r = project_.moveClip(clipId, destTrackId, newStart, sobrescribir);
    if (r.ok) seleccionarClip(clipId);  // la pista seleccionada pasa a ser la del clip
    validarSeleccion();                 // (sobrescribir puede haber eliminado clips)
    return r;
}

model::OpResult EditorController::moverSeleccion(model::TimeUs delta, bool sobrescribir) {
    if (seleccion_.clips.empty()) return model::OpResult::fallo("No hay ningún clip seleccionado.");
    model::OpResult r = project_.moveClips(seleccion_.clips, delta, sobrescribir);
    validarSeleccion();
    return r;
}

model::OpResult EditorController::splitAtPlayhead() {
    if (seleccion_.clips.empty()) return model::OpResult::fallo("No hay ningún clip seleccionado.");
    const model::Project antes = project_;  // para deshacer si algo falla
    bool alguno = false;
    for (model::Id id : seleccion_.clips) {
        // Un clip que ya no está bajo el cursor se salta: p. ej. el audio vinculado de un
        // video seleccionado, que se dividió junto con él.
        const model::Clip* c = project_.timeline().findClip(id);
        if (!c || playhead_ <= c->timelineStart || playhead_ >= c->timelineEnd()) continue;
        const model::OpResult r = project_.split(id, playhead_);
        if (!r.ok) {
            project_ = antes;
            return r;
        }
        alguno = true;
    }
    if (!alguno) return model::OpResult::fallo("El cursor no está dentro de ningún clip seleccionado.");
    return model::OpResult::exito();
}

model::OpResult EditorController::trimClip(model::Id clipId, model::TimeUs newIn, model::TimeUs newOut) {
    model::OpResult r = project_.trimClip(clipId, newIn, newOut);
    if (r.ok) seleccionarClip(clipId);
    return r;
}

model::OpResult EditorController::deleteSelectedClip(bool ripple) {
    if (seleccion_.clips.empty()) return model::OpResult::fallo("No hay ningún clip seleccionado.");
    // De derecha a izquierda: así el ripple de un clip no corre a los que faltan borrar.
    std::vector<model::Id> ids = seleccion_.clips;
    const model::Timeline& tl = project_.timeline();
    std::sort(ids.begin(), ids.end(),
              [&tl](model::Id a, model::Id b) { return tl.findClip(a)->timelineStart > tl.findClip(b)->timelineStart; });
    const model::Project antes = project_;
    for (model::Id id : ids) {
        if (!project_.timeline().findClip(id)) continue;  // ya se fue con su vinculado
        const model::OpResult r = project_.removeClip(id, ripple);
        if (!r.ok) {
            project_ = antes;
            return r;
        }
    }
    validarSeleccion();
    return model::OpResult::exito();
}

model::OpResult EditorController::unlinkAudio() {
    if (seleccion_.clipId == model::kSinId) return model::OpResult::fallo("No hay ningún clip seleccionado.");
    if (seleccion_.multiple()) return model::OpResult::fallo("Para desvincular, selecciona un solo clip.");
    return project_.unlinkAudio(seleccion_.clipId);
}

model::OpResult EditorController::setTrackMuted(model::Id trackId, bool muted) {
    return project_.setTrackMuted(trackId, muted);
}

void EditorController::seleccionarPista(model::Id trackId) {
    if (project_.timeline().findTrack(trackId)) {
        seleccion_.trackId = trackId;
        limpiarSeleccionClip();
    }
}

void EditorController::seleccionarClip(model::Id clipId) {
    const model::Track* pista = nullptr;
    if (project_.timeline().findClip(clipId, &pista)) {
        seleccion_.clipId = clipId;
        seleccion_.clips = {clipId};
        seleccion_.trackId = pista->id;
    }
}

void EditorController::alternarSeleccionClip(model::Id clipId) {
    const model::Track* pista = nullptr;
    if (!project_.timeline().findClip(clipId, &pista)) return;
    if (seleccion_.contiene(clipId)) {
        std::erase(seleccion_.clips, clipId);
        // El principal pasa a ser el último que queda seleccionado.
        seleccion_.clipId = seleccion_.clips.empty() ? model::kSinId : seleccion_.clips.back();
    } else {
        seleccion_.clips.push_back(clipId);
        seleccion_.clipId = clipId;
        seleccion_.trackId = pista->id;
    }
}

void EditorController::limpiarSeleccionClip() {
    seleccion_.clipId = model::kSinId;
    seleccion_.clips.clear();
}

void EditorController::setPlayhead(model::TimeUs t) {
    playhead_ = std::clamp<model::TimeUs>(t, 0, project_.timeline().duration());
    if (preview_) preview_->seek(playhead_);
}

// ---------------------------------------------------------------------------
// Preview
// ---------------------------------------------------------------------------

void EditorController::iniciarPreview(std::function<void()> aviso) {
    preview_ = std::make_unique<engine::PlaybackEngine>(std::move(aviso));
    revisionEnPreview_ = UINT64_MAX;
    sincronizarPreview();
    preview_->seek(playhead_);
}

void EditorController::sincronizarPreview() {
    if (!preview_ || revisionEnPreview_ == project_.revision()) return;
    revisionEnPreview_ = project_.revision();
    // El engine recibe una COPIA: puede leerla en su hilo sin bloquear el modelo.
    preview_->setTimeline({project_.timeline(), project_.media(), project_.canvas()});
    // Si el timeline se acortó, el cursor no puede quedar más allá del final.
    if (playhead_ > project_.timeline().duration()) setPlayhead(project_.timeline().duration());
}

void EditorController::reproducirPausar() {
    if (!preview_) return;
    if (preview_->reproduciendo()) {
        preview_->pause();
    } else {
        sincronizarPreview();  // por si quedaba un cambio sin enviar
        preview_->play();
    }
}

bool EditorController::reproduciendo() const {
    return preview_ && preview_->reproduciendo();
}

bool EditorController::actualizarPreview(engine::FrameBGRA& cuadro) {
    if (!preview_) return false;
    // Mientras se reproduce (y una vez más al terminar o pausar, para quedar en la
    // posición final exacta) el cursor sigue al reloj del preview.
    const bool reproduciendo = preview_->reproduciendo();
    if (reproduciendo || seguiaReloj_) playhead_ = preview_->reloj();
    seguiaReloj_ = reproduciendo;
    return preview_->tomarCuadro(cuadro);
}

std::string EditorController::errorPreview() const {
    return preview_ ? preview_->error() : std::string();
}

void EditorController::validarSeleccion() {
    const model::Timeline& tl = project_.timeline();
    std::erase_if(seleccion_.clips, [&tl](model::Id id) { return !tl.findClip(id); });
    if (!seleccion_.contiene(seleccion_.clipId))
        seleccion_.clipId = seleccion_.clips.empty() ? model::kSinId : seleccion_.clips.back();
    if (!tl.findTrack(seleccion_.trackId)) {
        const model::Track* v1 = tl.firstTrack(model::TrackKind::Video);
        seleccion_.trackId = v1 ? v1->id : model::kSinId;
    }
}

} // namespace controller
