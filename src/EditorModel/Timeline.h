// Timeline.h
// Capa: MODEL.
//
// El timeline es la lista de pistas. El orden del vector importa para las pistas de
// video: la primera pista de video (V1) es el fondo y las siguientes se dibujan encima.
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

    bool empty() const {
        for (const Track& t : tracks)
            if (!t.clips.empty()) return false;
        return true;
    }
};

} // namespace model
