// Timeline.h
// Capa: MODEL.
//
// El timeline es la lista de pistas, en este orden: pistas de video (V1 es el fondo y las
// siguientes se dibujan encima), pistas de audio de video (Audio V1, Audio V2...) y
// pistas de audio generales (A1, A2...).
// Es un tipo valor (copiable): el preview recibe una COPIA ("snapshot") para trabajar en
// otro hilo sin bloquear el modelo (design.md, D8).
#pragma once

#include "EditorModel/Track.h"

#include <vector>

namespace model {

struct Timeline {
    std::vector<Track> tracks;

    // Fin del timeline: el final del último clip de cualquier pista.
    TimeUs duration() const {
        TimeUs fin = 0;
        for (const Track& t : tracks)
            if (t.end() > fin) fin = t.end();
        return fin;
    }

    const Track* findTrack(Id trackId) const {
        for (const Track& t : tracks)
            if (t.id == trackId) return &t;
        return nullptr;
    }

    // Busca un clip en cualquier pista; si pista != nullptr, devuelve también su pista.
    const Clip* findClip(Id clipId, const Track** pista = nullptr) const {
        for (const Track& t : tracks) {
            if (const Clip* c = t.findClip(clipId)) {
                if (pista) *pista = &t;
                return c;
            }
        }
        return nullptr;
    }

    int countTracks(TrackKind kind) const {
        int n = 0;
        for (const Track& t : tracks)
            if (t.kind == kind) ++n;
        return n;
    }

    // Primera pista del tipo pedido (V1 o A1), o nullptr si no hay.
    const Track* firstTrack(TrackKind kind) const {
        for (const Track& t : tracks)
            if (t.kind == kind) return &t;
        return nullptr;
    }

    // Pista de audio que pertenece a la pista de video dada (o nullptr).
    const Track* audioTrackOf(Id videoTrackId) const {
        if (videoTrackId == kSinId) return nullptr;
        for (const Track& t : tracks)
            if (t.kind == TrackKind::Audio && t.videoTrackId == videoTrackId) return &t;
        return nullptr;
    }

    const Track* firstGeneralAudioTrack() const {
        for (const Track& t : tracks)
            if (t.esAudioGeneral()) return &t;
        return nullptr;
    }

    bool empty() const {
        for (const Track& t : tracks)
            if (!t.clips.empty()) return false;
        return true;
    }
};

} // namespace model
