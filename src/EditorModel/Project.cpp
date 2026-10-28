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
