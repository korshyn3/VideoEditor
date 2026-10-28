// Track.h
// Capa: MODEL.
//
// Una pista del timeline: de video o de audio. Sus clips se mantienen ordenados por
// timelineStart y no se solapan (salvo donde hay una transición).
#pragma once

#include "EditorModel/Clip.h"
#include "EditorModel/Ids.h"
#include "EditorModel/Transition.h"

#include <string>
#include <vector>

namespace model {

enum class TrackKind { Video, Audio };

struct Track {
    Id id = kSinId;
    TrackKind kind = TrackKind::Video;
    std::string name;   // "V1", "V2", "A1", ...
    bool muted = false; // solo tiene efecto en pistas de audio
    std::vector<Clip> clips;
    std::vector<Transition> transitions;

    // Fin del último clip (0 si la pista está vacía).
    TimeUs end() const {
        TimeUs fin = 0;
        for (const Clip& c : clips)
            if (c.timelineEnd() > fin) fin = c.timelineEnd();
        return fin;
    }

    const Clip* findClip(Id clipId) const {
        for (const Clip& c : clips)
            if (c.id == clipId) return &c;
        return nullptr;
    }
};

} // namespace model
