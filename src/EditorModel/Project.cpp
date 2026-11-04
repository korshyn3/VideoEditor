// Project.cpp
// Capa: MODEL. Ver Project.h.
#include "EditorModel/Project.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace model {

namespace {

Clip* buscarClip(Timeline& tl, Id id, Track** pista = nullptr) {
    for (Track& t : tl.tracks) {
        for (Clip& c : t.clips) {
            if (c.id == id) {
                if (pista) *pista = &t;
                return &c;
            }
        }
    }
    return nullptr;
}

Track* buscarPista(Timeline& tl, Id id) {
    for (Track& t : tl.tracks)
        if (t.id == id) return &t;
    return nullptr;
}

// Índice del clip en la pista (o -1).
int indiceDe(const Track& t, Id id) {
    for (size_t i = 0; i < t.clips.size(); ++i)
        if (t.clips[i].id == id) return static_cast<int>(i);
    return -1;
}

const Transition* transicionEntre(const Track& t, Id from, Id to) {
    for (const Transition& tr : t.transitions)
        if (tr.fromClipId == from && tr.toClipId == to) return &tr;
    return nullptr;
}

// Después de editar: cada transición dura lo que se solapan sus clips. Se quita si sus
// clips ya no están en la pista, no son consecutivos o dejaron de solaparse.
// (Los clips deben estar ordenados.)
void normalizarTransiciones(Track& t) {
    std::vector<Transition> validas;
    for (Transition tr : t.transitions) {
        const int i = indiceDe(t, tr.fromClipId), j = indiceDe(t, tr.toClipId);
        if (i < 0 || j != i + 1) continue;
        const TimeUs solape = t.clips[i].timelineEnd() - t.clips[j].timelineStart;
        if (solape <= 0) continue;
        tr.duration = solape;
        validas.push_back(tr);
    }
    t.transitions = std::move(validas);
}

// true si la pista es válida: los clips solo se solapan con el siguiente, y solo si hay
// una transición entre los dos (de esa duración), y las dos transiciones de un clip
// (entrada y salida) caben en él. (Los clips deben estar ordenados.)
bool sinSolapes(const Track& t) {
    TimeUs finAnteriores = 0;  // fin de los clips antes del anterior
    for (size_t i = 1; i < t.clips.size(); ++i) {
        const Clip& a = t.clips[i - 1];
        const Clip& b = t.clips[i];
        const Transition* tr = transicionEntre(t, a.id, b.id);
        const TimeUs permitido = tr ? tr->duration : 0;
        if (b.timelineStart < a.timelineEnd() - permitido || b.timelineStart < finAnteriores) return false;
        finAnteriores = std::max(finAnteriores, a.timelineEnd());
    }
    for (const Clip& c : t.clips) {
        TimeUs usado = 0;
        for (const Transition& tr : t.transitions)
            if (tr.fromClipId == c.id || tr.toClipId == c.id) usado += tr.duration;
        if (usado > c.duration()) return false;
    }
    return true;
}

// Ordena y normaliza todas las pistas y devuelve la primera que quedó inválida (o nullptr).
const Track* pistaInvalida(Timeline& tl) {
    for (Track& t : tl.tracks) {
        std::sort(t.clips.begin(), t.clips.end(),
                  [](const Clip& a, const Clip& b) { return a.timelineStart < b.timelineStart; });
        normalizarTransiciones(t);
    }
    for (const Track& t : tl.tracks)
        if (!sinSolapes(t)) return &t;
    return nullptr;
}

// Corre `delta` los clips de la pista que empiezan en `desde` o después, y sus clips
// vinculados (en otras pistas).
void correrDesde(Timeline& tl, const Track& pista, TimeUs desde, TimeUs delta) {
    std::vector<Id> ids;
    for (const Clip& c : pista.clips) {
        if (c.timelineStart < desde) continue;
        ids.push_back(c.id);
        if (c.vinculado()) ids.push_back(c.linkedClipId);
    }
    for (Track& t : tl.tracks)
        for (Clip& c : t.clips)
            if (std::find(ids.begin(), ids.end(), c.id) != ids.end()) c.timelineStart += delta;
}

// Ajusta los fades del clip a su duración (después de dividirlo o recortarlo): el fade
// in no pasa de la duración y el fade out no pasa de lo que deja el fade in. Los fades
// que quedan en 0 se quitan.
void ajustarFades(Clip& c) {
    const TimeUs dur = c.duration();
    for (Effect& e : c.effects) {
        if (Fade* f = std::get_if<Fade>(&e)) {
            f->fadeIn = std::min(f->fadeIn, dur);
            f->fadeOut = std::min(f->fadeOut, dur - f->fadeIn);
        }
    }
    std::erase_if(c.effects, [](const Effect& e) {
        const Fade* f = std::get_if<Fade>(&e);
        return f && f->fadeIn == 0 && f->fadeOut == 0;
    });
}

// Al dividir un clip, el fade in queda solo en la parte izquierda y el fade out solo en
// la derecha.
void repartirFades(Clip& izquierda, Clip& derecha) {
    for (Effect& e : izquierda.effects)
        if (Fade* f = std::get_if<Fade>(&e)) f->fadeOut = 0;
    for (Effect& e : derecha.effects)
        if (Fade* f = std::get_if<Fade>(&e)) f->fadeIn = 0;
    ajustarFades(izquierda);
    ajustarFades(derecha);
}

// Segundos con dos decimales, para los mensajes.
std::string segundosTexto(TimeUs t) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", tiempoASegundos(t));
    return buf;
}

// Valida un efecto ya limitado contra el clip al que se aplica.
OpResult validarEfecto(const Clip& c, const Effect& e) {
    if (const Fade* f = std::get_if<Fade>(&e)) {
        if (f->fadeIn + f->fadeOut > c.duration())
            return OpResult::fallo("El fade in (" + segundosTexto(f->fadeIn) + " s) más el fade out (" +
                                   segundosTexto(f->fadeOut) + " s) no pueden superar la duración del clip (" +
                                   segundosTexto(c.duration()) + " s).");
    }
    return OpResult::exito();
}

} // namespace

Project::Project() {
    Track v1;
    v1.id = newId();
    v1.kind = TrackKind::Video;
    v1.name = "V1";
    Track audioV1;
    audioV1.id = newId();
    audioV1.kind = TrackKind::Audio;
    audioV1.name = nombreAudioDe(v1);
    audioV1.videoTrackId = v1.id;
    Track a1;
    a1.id = newId();
    a1.kind = TrackKind::Audio;
    a1.name = "A1";
    timeline_.tracks = {v1, audioV1, a1};
}

const Track* Project::primeraPistaInvalida(Timeline& tl) {
    return pistaInvalida(tl);
}

void Project::setMediaOffline(Id mediaId, bool offline) {
    media_.setOffline(mediaId, offline);
    ++revision_;  // el render cambia (negro/silencio), pero no hay nada que guardar
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

void Project::sortClips(Track& track) {
    std::sort(track.clips.begin(), track.clips.end(),
              [](const Clip& a, const Clip& b) { return a.timelineStart < b.timelineStart; });
}

void Project::insertAudioTrackFor(const Track& pistaVideo) {
    Track a;
    a.id = newId();
    a.kind = TrackKind::Audio;
    a.name = nombreAudioDe(pistaVideo);
    a.videoTrackId = pistaVideo.id;
    // Después de la última pista de audio de video o, si no hay, de la última de video.
    auto despues = std::find_if(timeline_.tracks.rbegin(), timeline_.tracks.rend(), [](const Track& x) {
        return x.esAudioDeVideo() || x.kind == TrackKind::Video;
    });
    timeline_.tracks.insert(despues.base(), a);
}

void Project::repararVinculos() {
    for (Track& t : timeline_.tracks)
        for (Clip& c : t.clips)
            if (c.vinculado() && !timeline_.findClip(c.linkedClipId)) c.linkedClipId = kSinId;
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
        // Las pistas de video nuevas van después de la última de video (quedan "encima")
        // y traen su pista de audio de video.
        auto ultimaVideo = std::find_if(timeline_.tracks.rbegin(), timeline_.tracks.rend(),
                                        [](const Track& x) { return x.kind == TrackKind::Video; });
        timeline_.tracks.insert(ultimaVideo.base(), t);
        insertAudioTrackFor(t);
    } else {
        timeline_.tracks.push_back(t);  // pista de audio general, al final
    }
    if (nuevaPista) *nuevaPista = t.id;
    touch();
    return OpResult::exito();
}

OpResult Project::removeTrack(Id trackId) {
    const Track* t = timeline_.findTrack(trackId);
    if (!t) return OpResult::fallo("La pista no existe.");
    if (t->esAudioDeVideo()) {
        const Track* duena = timeline_.findTrack(t->videoTrackId);
        return OpResult::fallo("La pista " + t->name + " pertenece a la pista de video " +
                               (duena ? duena->name : std::string("?")) + " y solo se elimina junto con ella.");
    }
    if (t->kind == TrackKind::Video && timeline_.countTracks(TrackKind::Video) == 1)
        return OpResult::fallo("No se puede eliminar la única pista de video del proyecto.");

    // Una pista de video se lleva su pista de audio.
    const Track* audio = (t->kind == TrackKind::Video) ? timeline_.audioTrackOf(trackId) : nullptr;
    const Id audioId = audio ? audio->id : kSinId;
    std::erase_if(timeline_.tracks, [trackId, audioId](const Track& x) { return x.id == trackId || x.id == audioId; });
    repararVinculos();
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

    // Un video con audio lleva su clip de audio vinculado en la pista de audio de la pista.
    Track* pistaAudio = nullptr;
    if (track->kind == TrackKind::Video && info->hasAudio) {
        const Track* a = timeline_.audioTrackOf(track->id);
        if (!a) return OpResult::fallo("La pista " + track->name + " no tiene pista de audio.");
        pistaAudio = findTrackMut(a->id);
    }

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
    // Al final de la pista (y también después de lo que haya en su pista de audio, para
    // que el audio vinculado no se solape con un audio desvinculado).
    c.timelineStart = track->end();
    if (pistaAudio) c.timelineStart = std::max(c.timelineStart, pistaAudio->end());

    if (pistaAudio) {
        Clip a = c;
        a.id = newId();
        a.linkedClipId = c.id;
        c.linkedClipId = a.id;
        pistaAudio->clips.push_back(a);
        sortClips(*pistaAudio);
    }
    track->clips.push_back(c);
    sortClips(*track);

    if (nuevoClip) *nuevoClip = c.id;
    touch();
    return OpResult::exito();
}

OpResult Project::moveClip(Id clipId, Id destTrackId, TimeUs newStart, bool sobrescribir) {
    const Track* origen = nullptr;
    const Clip* clip = timeline_.findClip(clipId, &origen);
    if (!clip) return OpResult::fallo("El clip no existe.");
    if (newStart < 0) newStart = 0;

    // Un audio vinculado se mueve "a través" de su video: solo cambia el tiempo, y su
    // pista la decide la pista de video.
    if (clip->vinculado() && origen->kind == TrackKind::Audio) {
        const Track* pistaVideo = nullptr;
        const Clip* video = timeline_.findClip(clip->linkedClipId, &pistaVideo);
        if (video) return moveClip(video->id, pistaVideo->id, newStart, sobrescribir);
    }

    const Track* destino = timeline_.findTrack(destTrackId);
    if (!destino) return OpResult::fallo("La pista destino no existe.");
    if (destino->kind != origen->kind)
        return OpResult::fallo("Un clip solo se puede mover a una pista del mismo tipo (video o audio).");
    if (clip->vinculado() && !timeline_.audioTrackOf(destino->id))
        return OpResult::fallo("La pista destino no tiene pista de audio.");
    return aplicarMovimientos({{clipId, destTrackId, newStart}}, sobrescribir);
}

OpResult Project::moveClips(const std::vector<Id>& clipIds, TimeUs delta, bool sobrescribir) {
    // Cada clip se queda en su pista. Un audio vinculado se mueve a través de su video.
    std::vector<Destino> destinos;
    auto incluido = [&](Id id) {
        return std::any_of(destinos.begin(), destinos.end(), [id](const Destino& d) { return d.clipId == id; });
    };
    for (Id id : clipIds) {
        const Track* pista = nullptr;
        const Clip* c = timeline_.findClip(id, &pista);
        if (!c) return OpResult::fallo("Uno de los clips no existe.");
        if (c->vinculado() && pista->kind == TrackKind::Audio) {
            const Clip* video = timeline_.findClip(c->linkedClipId, &pista);
            if (video) c = video;
        }
        if (!incluido(c->id)) destinos.push_back({c->id, pista->id, c->timelineStart});
    }
    if (destinos.empty()) return OpResult::fallo("No hay clips que mover.");

    // El grupo no puede empezar antes de 0: el desplazamiento se limita para el primero.
    TimeUs inicioMinimo = destinos[0].start;
    for (const Destino& d : destinos) inicioMinimo = std::min(inicioMinimo, d.start);
    delta = std::max(delta, -inicioMinimo);
    for (Destino& d : destinos) d.start += delta;
    return aplicarMovimientos(std::move(destinos), sobrescribir);
}

namespace {

void quitarClip(Track& t, Id id) {
    std::erase_if(t.clips, [id](const Clip& c) { return c.id == id; });
    std::erase_if(t.transitions, [id](const Transition& tr) { return tr.fromClipId == id || tr.toClipId == id; });
}

} // namespace

void Project::cortarRango(Timeline& tl, Id clipId, TimeUs ini, TimeUs fin) {
    const Clip* c = buscarClip(tl, clipId);
    if (!c) return;
    const Id vinculado = c->linkedClipId;
    // Si se corta el medio, quedan dos partes: las derechas (del clip y de su vinculado)
    // son clips nuevos, vinculados entre sí.
    const bool medio = ini > c->timelineStart && fin < c->timelineEnd();
    const Id derecho = medio ? newId() : kSinId;
    const Id derechoVinculado = (medio && vinculado != kSinId) ? newId() : kSinId;

    auto cortar = [&](Id id, Id idDerecho, Id vinculoDerecho) {
        Track* t = nullptr;
        Clip* x = buscarClip(tl, id, &t);
        if (!x) return;
        const TimeUs xi = x->timelineStart, xf = x->timelineEnd();
        if (fin <= xi || ini >= xf) return;  // no se tocan
        if (ini <= xi && fin >= xf) {        // tapado por completo
            quitarClip(*t, id);
        } else if (ini <= xi) {              // tapado el inicio
            x->sourceIn += fin - xi;
            x->timelineStart = fin;
            ajustarFades(*x);
        } else if (fin >= xf) {              // tapado el final
            x->sourceOut -= xf - ini;
            ajustarFades(*x);
        } else {                             // tapado el medio: se divide
            Clip der = *x;
            der.id = idDerecho;
            der.linkedClipId = vinculoDerecho;
            der.sourceIn = x->sourceIn + (fin - xi);
            der.timelineStart = fin;
            x->sourceOut = x->sourceIn + (ini - xi);
            repartirFades(*x, der);
            for (Transition& tr : t->transitions)
                if (tr.fromClipId == id) tr.fromClipId = idDerecho;
            t->clips.push_back(der);  // (invalida `x`)
            sortClips(*t);
        }
    };
    cortar(clipId, derecho, derechoVinculado);
    if (vinculado != kSinId) cortar(vinculado, derechoVinculado, derecho);
}

OpResult Project::aplicarMovimientos(std::vector<Destino> destinos, bool sobrescribir) {
    // Los audios vinculados acompañan a su video, a la pista de audio de su pista destino.
    const size_t n = destinos.size();
    for (size_t i = 0; i < n; ++i) {
        const Clip* c = timeline_.findClip(destinos[i].clipId);
        if (!c || !c->vinculado()) continue;
        const Track* pistaAudio = timeline_.audioTrackOf(destinos[i].trackId);
        if (!pistaAudio) return OpResult::fallo("La pista destino no tiene pista de audio.");
        destinos.push_back({c->linkedClipId, pistaAudio->id, destinos[i].start});
    }

    // Se trabaja sobre una copia del timeline y solo se acepta si queda válida.
    Timeline nuevo = timeline_;
    struct Colocacion {
        Clip clip;
        Id trackId;
    };
    std::vector<Colocacion> colocar;
    for (const Destino& d : destinos) {
        Track* origen = nullptr;
        Clip* c = buscarClip(nuevo, d.clipId, &origen);
        if (!c) return OpResult::fallo("El clip no existe.");
        Clip copia = *c;
        copia.timelineStart = d.start;
        colocar.push_back({copia, d.trackId});
        std::erase_if(origen->clips, [&d](const Clip& x) { return x.id == d.clipId; });
    }

    // Sobrescribir: en cada pista destino se libera el tramo que ocupará cada clip movido.
    if (sobrescribir) {
        for (const Colocacion& col : colocar) {
            Track* t = buscarPista(nuevo, col.trackId);
            if (!t) continue;
            std::vector<Id> debajo;
            for (const Clip& x : t->clips)
                if (x.timelineStart < col.clip.timelineEnd() && col.clip.timelineStart < x.timelineEnd())
                    debajo.push_back(x.id);
            for (Id id : debajo) cortarRango(nuevo, id, col.clip.timelineStart, col.clip.timelineEnd());
        }
        // Los restos de menos de un cuadro se descartan (con su vinculado).
        const TimeUs cuadro = frameDuration();
        for (Track& t : nuevo.tracks)
            std::erase_if(t.clips, [cuadro](const Clip& x) { return x.duration() < cuadro; });
        for (Track& t : nuevo.tracks)
            for (Clip& x : t.clips)
                if (x.vinculado() && !buscarClip(nuevo, x.linkedClipId)) x.linkedClipId = kSinId;
    }

    for (const Colocacion& col : colocar) {
        Track* t = buscarPista(nuevo, col.trackId);
        if (!t) return OpResult::fallo("La pista destino no existe.");
        t->clips.push_back(col.clip);
    }
    if (const Track* mala = pistaInvalida(nuevo))
        return OpResult::fallo("El movimiento haría que un clip se solape con otro en la pista " + mala->name +
                               ". (Con Shift se sobrescriben los clips de debajo.)");
    confirmar(nuevo);
    return OpResult::exito();
}

void Project::confirmar(Timeline& nuevo) {
    for (size_t i = 0; i < timeline_.tracks.size(); ++i) {
        timeline_.tracks[i].clips = std::move(nuevo.tracks[i].clips);
        timeline_.tracks[i].transitions = std::move(nuevo.tracks[i].transitions);
    }
    touch();
}

OpResult Project::split(Id clipId, TimeUs t, Id* clipDerecho) {
    const Clip* clip = timeline_.findClip(clipId);
    if (!clip) return OpResult::fallo("El clip no existe.");
    // Estrictamente dentro: en un borde (o fuera) no hay nada que dividir.
    if (t <= clip->timelineStart || t >= clip->timelineEnd())
        return OpResult::fallo("El cursor no está dentro del clip seleccionado.");
    // Dentro del tramo de una transición (donde el clip se solapa con su vecino) no se
    // puede dividir.
    for (const Track& pista : timeline_.tracks) {
        for (const Transition& tr : pista.transitions) {
            const Clip* from = pista.findClip(tr.fromClipId);
            const Clip* to = pista.findClip(tr.toClipId);
            if (!from || !to || (from->id != clipId && to->id != clipId && from->id != clip->linkedClipId &&
                                 to->id != clip->linkedClipId))
                continue;
            if (t >= to->timelineStart && t < from->timelineEnd())
                return OpResult::fallo("No se puede dividir dentro de una transición.");
        }
    }

    const Id vinculado = clip->linkedClipId;
    const Id derecho = newId();
    const Id derechoVinculado = (vinculado != kSinId) ? newId() : kSinId;

    // Parte un clip en t: el original queda como la mitad izquierda.
    auto partir = [&](Id id, Id idDerecho, Id vinculoDerecho) {
        Track* track = nullptr;
        Clip* c = findClipMut(id, &track);
        if (!c) return;
        // El punto de corte en la fuente se calcula con la misma distancia que en el
        // timeline (enteros: las dos mitades suman exactamente el tramo original).
        const TimeUs corteFuente = c->sourceIn + (t - c->timelineStart);
        Clip der = *c;  // copia medio, efectos y transformación PiP
        der.id = idDerecho;
        der.sourceIn = corteFuente;
        der.timelineStart = t;
        der.linkedClipId = vinculoDerecho;
        c->sourceOut = corteFuente;
        repartirFades(*c, der);
        // Una transición que salía del clip original ahora sale de su parte derecha.
        for (Transition& tr : track->transitions)
            if (tr.fromClipId == id) tr.fromClipId = idDerecho;
        track->clips.push_back(der);  // (invalida `c`)
        sortClips(*track);
    };
    partir(clipId, derecho, derechoVinculado);
    if (vinculado != kSinId) partir(vinculado, derechoVinculado, derecho);

    if (clipDerecho) *clipDerecho = derecho;
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

    // El clip vinculado recibe el mismo recorte (tienen el mismo tramo y posición). Se
    // trabaja sobre una copia: una transición del borde recortado cambia de duración (o
    // desaparece si los clips dejan de solaparse).
    Timeline nuevo = timeline_;
    for (Id id : {clipId, clip->linkedClipId}) {
        Clip* c = (id != kSinId) ? buscarClip(nuevo, id) : nullptr;
        if (!c) continue;
        c->sourceIn = newIn;
        c->sourceOut = newOut;
        c->timelineStart = newStart;
        ajustarFades(*c);
    }
    if (const Track* mala = pistaInvalida(nuevo)) {
        if (mala->id == track->id)
            return OpResult::fallo("El recorte haría que el clip se solape con un clip vecino.");
        return OpResult::fallo("El recorte haría que el clip vinculado se solape con un clip de la pista " +
                               mala->name + ".");
    }
    confirmar(nuevo);
    return OpResult::exito();
}

OpResult Project::removeClip(Id clipId, bool ripple) {
    const Track* pista = nullptr;
    const Clip* clip = timeline_.findClip(clipId, &pista);
    if (!clip) return OpResult::fallo("El clip no existe.");
    const TimeUs fin = clip->timelineEnd();
    const TimeUs duracion = clip->duration();
    const Id vinculado = clip->linkedClipId;
    const Track* pistaVinculado = nullptr;
    if (vinculado != kSinId) timeline_.findClip(vinculado, &pistaVinculado);

    // Se trabaja sobre una copia del timeline y solo se acepta si queda válida.
    Timeline nuevo = timeline_;
    std::vector<Id> pistasAfectadas = {pista->id};
    if (pistaVinculado) pistasAfectadas.push_back(pistaVinculado->id);

    for (Track& t : nuevo.tracks) {
        std::erase_if(t.transitions, [&](const Transition& tr) {
            return tr.fromClipId == clipId || tr.toClipId == clipId || tr.fromClipId == vinculado ||
                   tr.toClipId == vinculado;
        });
        std::erase_if(t.clips, [&](const Clip& c) { return c.id == clipId || (vinculado != kSinId && c.id == vinculado); });
    }

    if (ripple) {
        // Clips a correr: los posteriores en las pistas afectadas y, para que ningún
        // vínculo se rompa, también sus clips vinculados.
        std::vector<Id> correr;
        for (const Track& t : nuevo.tracks) {
            if (std::find(pistasAfectadas.begin(), pistasAfectadas.end(), t.id) == pistasAfectadas.end()) continue;
            for (const Clip& c : t.clips) {
                if (c.timelineStart < fin) continue;
                correr.push_back(c.id);
                if (c.vinculado()) correr.push_back(c.linkedClipId);
            }
        }
        std::sort(correr.begin(), correr.end());
        correr.erase(std::unique(correr.begin(), correr.end()), correr.end());
        for (Track& t : nuevo.tracks) {
            for (Clip& c : t.clips)
                if (std::binary_search(correr.begin(), correr.end(), c.id)) c.timelineStart -= duracion;
            sortClips(t);
        }
        if (const Track* mala = pistaInvalida(nuevo))
            return OpResult::fallo("Al cerrar el hueco, un clip vinculado se solaparía con otro clip de la pista " +
                                   mala->name + ". Usa la eliminación normal.");
    }
    confirmar(nuevo);
    return OpResult::exito();
}

OpResult Project::unlinkAudio(Id clipId) {
    Clip* clip = findClipMut(clipId);
    if (!clip) return OpResult::fallo("El clip no existe.");
    if (!clip->vinculado()) return OpResult::fallo("El clip no tiene audio vinculado.");
    if (Clip* otro = findClipMut(clip->linkedClipId)) otro->linkedClipId = kSinId;
    clip->linkedClipId = kSinId;
    touch();
    return OpResult::exito();
}

struct Project::DatosTransicion {
    const Track* pista = nullptr;
    const Clip* from = nullptr;
    const Clip* to = nullptr;              // clip siguiente en la pista
    const Transition* actual = nullptr;    // transición que ya sale de `from` (o nullptr)
};

OpResult Project::datosTransicion(Id fromClipId, DatosTransicion& d) const {
    d.from = timeline_.findClip(fromClipId, &d.pista);
    if (!d.from) return OpResult::fallo("El clip no existe.");
    if (d.pista->kind != TrackKind::Video)
        return OpResult::fallo("Las transiciones se agregan entre clips de video.");
    const int i = indiceDe(*d.pista, fromClipId);
    if (i + 1 >= static_cast<int>(d.pista->clips.size()))
        return OpResult::fallo("No hay ningún clip después de este en la pista " + d.pista->name + ".");
    d.to = &d.pista->clips[i + 1];
    d.actual = transicionEntre(*d.pista, d.from->id, d.to->id);
    return OpResult::exito();
}

const Transition* Project::transitionFrom(Id fromClipId) const {
    for (const Track& t : timeline_.tracks)
        for (const Transition& tr : t.transitions)
            if (tr.fromClipId == fromClipId) return &tr;
    return nullptr;
}

TimeUs Project::maxTransitionDuration(Id fromClipId) const {
    DatosTransicion d;
    if (!datosTransicion(fromClipId, d)) return 0;
    // Cada clip cede lo que no usa su otra transición (la de entrada de `from`, la de
    // salida de `to`).
    TimeUs otraFrom = 0, otraTo = 0;
    for (const Transition& tr : d.pista->transitions) {
        if (tr.toClipId == d.from->id) otraFrom = tr.duration;
        if (tr.fromClipId == d.to->id) otraTo = tr.duration;
    }
    return std::max<TimeUs>(0, std::min(d.from->duration() - otraFrom, d.to->duration() - otraTo));
}

namespace {

// Transición espejo entre los audios vinculados de una transición de video (o nullptr).
Transition* espejoDe(Timeline& tl, const Clip& from, const Clip& to, Track** pistaAudio = nullptr) {
    if (!from.vinculado() || !to.vinculado()) return nullptr;
    for (Track& t : tl.tracks) {
        for (Transition& tr : t.transitions) {
            if (tr.fromClipId == from.linkedClipId && tr.toClipId == to.linkedClipId) {
                if (pistaAudio) *pistaAudio = &t;
                return &tr;
            }
        }
    }
    return nullptr;
}

} // namespace

OpResult Project::addTransition(Id fromClipId, TransitionStyle estilo, TimeUs duracion) {
    DatosTransicion d;
    if (OpResult r = datosTransicion(fromClipId, d); !r) return r;
    if (d.actual) return OpResult::fallo("Ya hay una transición entre estos clips; cámbiala o quítala.");
    if (d.to->timelineStart != d.from->timelineEnd())
        return OpResult::fallo("Los clips deben estar contiguos (sin hueco entre ellos) para agregar una transición.");
    if (duracion < frameDuration()) return OpResult::fallo("La transición debe durar al menos un cuadro.");
    const TimeUs maximo = maxTransitionDuration(fromClipId);
    if (duracion > maximo)
        return OpResult::fallo("La transición no puede durar más de " + segundosTexto(maximo) +
                               " s (lo que permiten los dos clips).");

    // Sobre una copia: el clip siguiente y los posteriores (con sus audios) se adelantan.
    Timeline nuevo = timeline_;
    Track* pista = buscarPista(nuevo, d.pista->id);
    const Clip from = *d.from, to = *d.to;
    correrDesde(nuevo, *pista, to.timelineStart, -duracion);
    pista->transitions.push_back({from.id, to.id, estilo, duracion});
    // Si los dos tienen audio vinculado (en la misma pista de audio), la transición espejo.
    if (from.vinculado() && to.vinculado()) {
        const Track* pa = nullptr;
        const Track* pb = nullptr;
        nuevo.findClip(from.linkedClipId, &pa);
        nuevo.findClip(to.linkedClipId, &pb);
        if (pa && pa == pb) buscarPista(nuevo, pa->id)->transitions.push_back({from.linkedClipId, to.linkedClipId, estilo, duracion});
    }
    if (const Track* mala = pistaInvalida(nuevo))
        return OpResult::fallo("Al adelantar los clips siguientes, un clip se solaparía con otro en la pista " +
                               mala->name + ".");
    confirmar(nuevo);
    return OpResult::exito();
}

OpResult Project::setTransition(Id fromClipId, TransitionStyle estilo, TimeUs duracion) {
    DatosTransicion d;
    if (OpResult r = datosTransicion(fromClipId, d); !r) return r;
    if (!d.actual) return OpResult::fallo("No hay ninguna transición que salga de este clip.");
    if (duracion < frameDuration()) return OpResult::fallo("La transición debe durar al menos un cuadro.");
    const TimeUs maximo = maxTransitionDuration(fromClipId);
    if (duracion > maximo)
        return OpResult::fallo("La transición no puede durar más de " + segundosTexto(maximo) +
                               " s (lo que permiten los dos clips).");

    Timeline nuevo = timeline_;
    Track* pista = buscarPista(nuevo, d.pista->id);
    const Clip from = *d.from, to = *d.to;
    const TimeUs diferencia = duracion - d.actual->duration;
    correrDesde(nuevo, *pista, to.timelineStart, -diferencia);
    for (Transition& tr : pista->transitions) {
        if (tr.fromClipId == from.id) {
            tr.style = estilo;
            tr.duration = duracion;
        }
    }
    if (Transition* espejo = espejoDe(nuevo, from, to)) {
        espejo->style = estilo;
        espejo->duration = duracion;
    }
    if (const Track* mala = pistaInvalida(nuevo))
        return OpResult::fallo("Al correr los clips siguientes, un clip se solaparía con otro en la pista " +
                               mala->name + ".");
    confirmar(nuevo);
    return OpResult::exito();
}

OpResult Project::removeTransition(Id fromClipId) {
    DatosTransicion d;
    if (OpResult r = datosTransicion(fromClipId, d); !r) return r;
    if (!d.actual) return OpResult::fallo("No hay ninguna transición que salga de este clip.");

    // El clip siguiente y los posteriores vuelven a correrse la duración de la transición.
    Timeline nuevo = timeline_;
    Track* pista = buscarPista(nuevo, d.pista->id);
    const Clip from = *d.from, to = *d.to;
    const TimeUs duracion = d.actual->duration;
    Track* pistaAudio = nullptr;
    if (espejoDe(nuevo, from, to, &pistaAudio))
        std::erase_if(pistaAudio->transitions, [&](const Transition& tr) { return tr.fromClipId == from.linkedClipId; });
    std::erase_if(pista->transitions, [&](const Transition& tr) { return tr.fromClipId == from.id; });
    correrDesde(nuevo, *pista, to.timelineStart, duracion);
    if (const Track* mala = pistaInvalida(nuevo))
        return OpResult::fallo("Al correr los clips siguientes, un clip se solaparía con otro en la pista " +
                               mala->name + ".");
    confirmar(nuevo);
    return OpResult::exito();
}

OpResult Project::setPipTransform(Id clipId, PipTransform transform) {
    Track* pista = nullptr;
    Clip* clip = findClipMut(clipId, &pista);
    if (!clip) return OpResult::fallo("El clip no existe.");
    if (pista->kind != TrackKind::Video) return OpResult::fallo("Solo los clips de video tienen tamaño y posición.");
    transform.x = std::clamp(transform.x, 0.0, 1.0);
    transform.y = std::clamp(transform.y, 0.0, 1.0);
    transform.scale = std::clamp(transform.scale, PipTransform::kEscalaMin, 1.0);
    transform.opacity = std::clamp(transform.opacity, 0.0, 1.0);
    if (transform == clip->transform) return OpResult::exito();
    clip->transform = transform;
    touch();
    return OpResult::exito();
}

OpResult Project::clipConEfectos(Id clipId, Clip** clip) {
    Track* pista = nullptr;
    *clip = findClipMut(clipId, &pista);
    if (!*clip) return OpResult::fallo("El clip no existe.");
    if (pista->kind != TrackKind::Video) return OpResult::fallo("Los efectos solo se aplican a clips de video.");
    return OpResult::exito();
}

OpResult Project::addEffect(Id clipId, Effect efecto) {
    Clip* clip = nullptr;
    if (OpResult r = clipConEfectos(clipId, &clip); !r) return r;
    efecto = limitarEfecto(efecto);
    if (OpResult r = validarEfecto(*clip, efecto); !r) return r;
    clip->effects.push_back(efecto);
    touch();
    return OpResult::exito();
}

OpResult Project::setEffect(Id clipId, size_t indice, Effect efecto) {
    Clip* clip = nullptr;
    if (OpResult r = clipConEfectos(clipId, &clip); !r) return r;
    if (indice >= clip->effects.size()) return OpResult::fallo("El efecto no existe.");
    if (clip->effects[indice].index() != efecto.index()) return OpResult::fallo("El efecto es de otro tipo.");
    efecto = limitarEfecto(efecto);
    if (OpResult r = validarEfecto(*clip, efecto); !r) return r;
    clip->effects[indice] = efecto;
    touch();
    return OpResult::exito();
}

OpResult Project::moveEffect(Id clipId, size_t indice, int delta) {
    Clip* clip = nullptr;
    if (OpResult r = clipConEfectos(clipId, &clip); !r) return r;
    const long long destino = static_cast<long long>(indice) + delta;
    if (indice >= clip->effects.size() || destino < 0 || destino >= static_cast<long long>(clip->effects.size()))
        return OpResult::fallo("El efecto no se puede mover más.");
    std::swap(clip->effects[indice], clip->effects[static_cast<size_t>(destino)]);
    touch();
    return OpResult::exito();
}

OpResult Project::removeEffect(Id clipId, size_t indice) {
    Clip* clip = nullptr;
    if (OpResult r = clipConEfectos(clipId, &clip); !r) return r;
    if (indice >= clip->effects.size()) return OpResult::fallo("El efecto no existe.");
    clip->effects.erase(clip->effects.begin() + static_cast<std::ptrdiff_t>(indice));
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
