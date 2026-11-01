// Project.cpp
// Capa: MODEL. Ver Project.h.
#include "EditorModel/Project.h"

#include <algorithm>
#include <cctype>

namespace model {

Project::Project() {
    Track v1;
    v1.id = newId();
    v1.kind = TrackKind::Video;
    v1.name = "V1";
    Track a1;
    a1.id = newId();
    a1.kind = TrackKind::Audio;
    a1.name = "A1";
    timeline_.tracks = {v1, a1};
}

void Project::touch() {
    ++revision_;
    dirty_ = true;
}

Track* Project::findTrackMut(Id trackId) {
    for (Track& t : timeline_.tracks)
        if (t.id == trackId) return &t;
    return nullptr;
}

std::string Project::nextTrackName(TrackKind kind) const {
    // Siguiente número después del mayor existente (así no se repiten nombres aunque se
    // hayan borrado pistas intermedias).
    const char prefijo = (kind == TrackKind::Video) ? 'V' : 'A';
    int mayor = 0;
    for (const Track& t : timeline_.tracks) {
        if (t.kind != kind || t.name.size() < 2 || t.name[0] != prefijo) continue;
        if (!std::all_of(t.name.begin() + 1, t.name.end(), [](unsigned char c) { return std::isdigit(c); }))
            continue;
        mayor = std::max(mayor, std::stoi(t.name.substr(1)));
    }
    return std::string(1, prefijo) + std::to_string(mayor + 1);
}

Clip* Project::findClipMut(Id clipId, Track** pista) {
    for (Track& t : timeline_.tracks) {
        for (Clip& c : t.clips) {
            if (c.id == clipId) {
                if (pista) *pista = &t;
                return &c;
            }
        }
    }
    return nullptr;
}

TimeUs Project::frameDuration() const {
    // 1 cuadro = den/num segundos, redondeado a microsegundos (como mínimo 1 µs).
    const Rational& f = canvas_.fps;
    if (!f.valido()) return kMicrosPorSegundo / 30;
    return std::max<TimeUs>(1, (kMicrosPorSegundo * f.den + f.num / 2) / f.num);
}

bool Project::overlaps(const Track& track, TimeUs start, TimeUs end, Id ignorar) {
    for (const Clip& c : track.clips) {
        if (c.id == ignorar) continue;
        if (start < c.timelineEnd() && c.timelineStart < end) return true;
    }
    return false;
}

void Project::sortClips(Track& track) {
    std::sort(track.clips.begin(), track.clips.end(),
              [](const Clip& a, const Clip& b) { return a.timelineStart < b.timelineStart; });
}

Id Project::addMedia(MediaInfo info) {
    const Id id = newId();
    media_.add(id, std::move(info));
    touch();
    return id;
}

OpResult Project::addTrack(TrackKind kind, Id* nuevaPista) {
    Track t;
    t.id = newId();
    t.kind = kind;
    t.name = nextTrackName(kind);

    if (kind == TrackKind::Video) {
        // Las pistas de video nuevas van después de la última de video (quedan "encima").
        auto ultimaVideo = std::find_if(timeline_.tracks.rbegin(), timeline_.tracks.rend(),
                                        [](const Track& x) { return x.kind == TrackKind::Video; });
        timeline_.tracks.insert(ultimaVideo.base(), t);
    } else {
        timeline_.tracks.push_back(t);
    }
    if (nuevaPista) *nuevaPista = t.id;
    touch();
    return OpResult::exito();
}

OpResult Project::removeTrack(Id trackId) {
    const Track* t = timeline_.findTrack(trackId);
    if (!t) return OpResult::fallo("La pista no existe.");
    if (t->kind == TrackKind::Video && timeline_.countTracks(TrackKind::Video) == 1)
        return OpResult::fallo("No se puede eliminar la única pista de video del proyecto.");

    std::erase_if(timeline_.tracks, [trackId](const Track& x) { return x.id == trackId; });
    touch();
    return OpResult::exito();
}

OpResult Project::setTrackMuted(Id trackId, bool muted) {
    Track* t = findTrackMut(trackId);
    if (!t) return OpResult::fallo("La pista no existe.");
    if (t->kind != TrackKind::Audio) return OpResult::fallo("Solo se pueden silenciar las pistas de audio.");
    if (t->muted == muted) return OpResult::exito();
    t->muted = muted;
    touch();
    return OpResult::exito();
}

OpResult Project::appendMedia(Id trackId, Id mediaId, Id* nuevoClip) {
    Track* track = findTrackMut(trackId);
    if (!track) return OpResult::fallo("La pista no existe.");
    const MediaInfo* info = media_.find(mediaId);
    if (!info) return OpResult::fallo("El medio no existe en el proyecto.");

    if (track->kind == TrackKind::Video && !info->hasVideo)
        return OpResult::fallo("\"" + info->path + "\" no tiene video; colócalo en una pista de audio.");
    if (track->kind == TrackKind::Audio && !info->hasAudio)
        return OpResult::fallo("\"" + info->path + "\" no tiene audio y no se puede colocar en una pista de audio.");
    if (info->duration <= 0)
        return OpResult::fallo("\"" + info->path + "\" tiene duración cero.");

    // El primer video de un proyecto vacío define el lienzo (resolución y fps).
    if (timeline_.empty() && info->hasVideo && track->kind == TrackKind::Video) {
        canvas_.width = info->width;
        canvas_.height = info->height;
        if (info->fps.valido()) canvas_.fps = info->fps;
    }

    Clip c;
    c.id = newId();
    c.mediaId = mediaId;
    c.sourceIn = 0;
    c.sourceOut = info->duration;
    c.timelineStart = track->end();
    c.linkedAudio = (track->kind == TrackKind::Video) && info->hasAudio;
    track->clips.push_back(c);
    sortClips(*track);

    if (nuevoClip) *nuevoClip = c.id;
    touch();
    return OpResult::exito();
}

OpResult Project::moveClip(Id clipId, Id destTrackId, TimeUs newStart) {
    const Track* origen = nullptr;
    const Clip* clip = timeline_.findClip(clipId, &origen);
    if (!clip) return OpResult::fallo("El clip no existe.");
    Track* destino = findTrackMut(destTrackId);
    if (!destino) return OpResult::fallo("La pista destino no existe.");
    if (destino->kind != origen->kind)
        return OpResult::fallo("Un clip solo se puede mover a una pista del mismo tipo (video o audio).");

    if (newStart < 0) newStart = 0;
    if (overlaps(*destino, newStart, newStart + clip->duration(), clipId))
        return OpResult::fallo("El clip se solaparía con otro clip de la pista.");

    Clip copia = *clip;
    copia.timelineStart = newStart;
    Track* origenMut = findTrackMut(origen->id);
    std::erase_if(origenMut->clips, [clipId](const Clip& c) { return c.id == clipId; });
    destino->clips.push_back(copia);
    sortClips(*destino);
    touch();
    return OpResult::exito();
}

OpResult Project::split(Id clipId, TimeUs t, Id* clipDerecho) {
    Track* track = nullptr;
    Clip* clip = findClipMut(clipId, &track);
    if (!clip) return OpResult::fallo("El clip no existe.");
    // Estrictamente dentro: en un borde (o fuera) no hay nada que dividir.
    if (t <= clip->timelineStart || t >= clip->timelineEnd())
        return OpResult::fallo("El cursor no está dentro del clip seleccionado.");

    // El punto de corte en la fuente se calcula con la misma distancia que en el timeline
    // (enteros: las dos mitades suman exactamente el tramo original).
    const TimeUs corteFuente = clip->sourceIn + (t - clip->timelineStart);

    Clip derecho = *clip;  // copia medio, efectos, audio vinculado y transformación PiP
    derecho.id = newId();
    derecho.sourceIn = corteFuente;
    derecho.timelineStart = t;
    clip->sourceOut = corteFuente;

    // Una transición que salía del clip original ahora sale de su parte derecha.
    for (Transition& tr : track->transitions)
        if (tr.fromClipId == clipId) tr.fromClipId = derecho.id;

    track->clips.push_back(derecho);
    sortClips(*track);
    if (clipDerecho) *clipDerecho = derecho.id;
    touch();
    return OpResult::exito();
}

OpResult Project::trimClip(Id clipId, TimeUs newIn, TimeUs newOut) {
    Track* track = nullptr;
    Clip* clip = findClipMut(clipId, &track);
    if (!clip) return OpResult::fallo("El clip no existe.");
    const MediaInfo* info = media_.find(clip->mediaId);
    const TimeUs duracionFuente = info ? info->duration : clip->sourceOut;

    // Límites de la fuente; además el clip no puede empezar antes de 0 en el timeline.
    const TimeUs minIn = std::max<TimeUs>(0, clip->sourceIn - clip->timelineStart);
    newIn = std::clamp<TimeUs>(newIn, minIn, duracionFuente);
    newOut = std::clamp<TimeUs>(newOut, 0, duracionFuente);
    if (newOut - newIn < frameDuration())
        return OpResult::fallo("El clip debe durar al menos un cuadro.");

    const TimeUs newStart = clip->timelineStart + (newIn - clip->sourceIn);
    if (overlaps(*track, newStart, newStart + (newOut - newIn), clipId))
        return OpResult::fallo("El recorte haría que el clip se solape con un clip vecino.");

    clip->sourceIn = newIn;
    clip->sourceOut = newOut;
    clip->timelineStart = newStart;
    touch();
    return OpResult::exito();
}

OpResult Project::removeClip(Id clipId, bool ripple) {
    Track* track = nullptr;
    const Clip* clip = findClipMut(clipId, &track);
    if (!clip) return OpResult::fallo("El clip no existe.");
    const TimeUs fin = clip->timelineEnd();
    const TimeUs duracion = clip->duration();

    std::erase_if(track->transitions,
                  [clipId](const Transition& tr) { return tr.fromClipId == clipId || tr.toClipId == clipId; });
    std::erase_if(track->clips, [clipId](const Clip& c) { return c.id == clipId; });  // clip queda inválido

    if (ripple) {
        // Solo los clips que empiezan después del eliminado (en la misma pista).
        for (Clip& c : track->clips)
            if (c.timelineStart >= fin) c.timelineStart -= duracion;
    }
    touch();
    return OpResult::exito();
}

OpResult Project::separateAudio(Id clipId, Id* clipAudio) {
    const Track* pistaVideo = nullptr;
    const Clip* clip = timeline_.findClip(clipId, &pistaVideo);
    if (!clip) return OpResult::fallo("El clip no existe.");
    if (pistaVideo->kind != TrackKind::Video)
        return OpResult::fallo("Solo se puede separar el audio de un clip de video.");
    const MediaInfo* info = media_.find(clip->mediaId);
    if (!info || !info->hasAudio) return OpResult::fallo("El clip no tiene audio que separar.");
    if (!clip->linkedAudio) return OpResult::fallo("El audio de este clip ya está separado.");

    // Clip de audio: mismo medio, tramo y posición (sin efectos: son de video).
    Clip audio;
    audio.id = newId();
    audio.mediaId = clip->mediaId;
    audio.sourceIn = clip->sourceIn;
    audio.sourceOut = clip->sourceOut;
    audio.timelineStart = clip->timelineStart;

    // Primera pista de audio libre en ese rango; si no hay, una nueva.
    Track* destino = nullptr;
    for (Track& t : timeline_.tracks) {
        if (t.kind == TrackKind::Audio && !overlaps(t, audio.timelineStart, audio.timelineEnd(), kSinId)) {
            destino = &t;
            break;
        }
    }
    Track nueva;
    if (!destino) {
        nueva.id = newId();
        nueva.kind = TrackKind::Audio;
        nueva.name = nextTrackName(TrackKind::Audio);
    }

    // A partir de aquí ya no puede fallar. (Agregar una pista puede mover el vector de
    // pistas en memoria, por eso el clip de video se busca de nuevo al final.)
    if (!destino) {
        timeline_.tracks.push_back(nueva);
        destino = &timeline_.tracks.back();
    }
    destino->clips.push_back(audio);
    sortClips(*destino);
    findClipMut(clipId)->linkedAudio = false;

    if (clipAudio) *clipAudio = audio.id;
    touch();
    return OpResult::exito();
}

OpResult Project::setCanvas(const CanvasSettings& canvas) {
    if (canvas.width < 16 || canvas.height < 16 || canvas.width > 7680 || canvas.height > 7680)
        return OpResult::fallo("La resolución del proyecto debe estar entre 16 y 7680 píxeles.");
    if (!canvas.fps.valido())
        return OpResult::fallo("Los fps del proyecto no son válidos.");
    canvas_ = canvas;
    touch();
    return OpResult::exito();
}

void Project::setExportSettings(const ExportSettings& settings) {
    export_ = settings;
    touch();
}

} // namespace model
