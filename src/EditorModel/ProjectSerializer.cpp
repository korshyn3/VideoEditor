// ProjectSerializer.cpp
// Capa: MODEL. Ver ProjectSerializer.h.
#include "EditorModel/ProjectSerializer.h"

#include <nlohmann/json.hpp>

#include <set>
#include <stdexcept>

namespace model {

using nlohmann::json;

namespace {

// Error de formato con un mensaje para el usuario (se atrapa en fromJson).
struct ErrorFormato : std::runtime_error {
    using std::runtime_error::runtime_error;
};

[[noreturn]] void fallar(const std::string& mensaje) {
    throw ErrorFormato(mensaje);
}

// Campo obligatorio de un objeto.
const json& campo(const json& j, const char* nombre, const std::string& donde) {
    if (!j.is_object() || !j.contains(nombre)) fallar("Falta el campo \"" + std::string(nombre) + "\" en " + donde + ".");
    return j.at(nombre);
}

template <class T>
T leer(const json& j, const char* nombre, const std::string& donde) {
    try {
        return campo(j, nombre, donde).get<T>();
    } catch (const json::exception&) {
        fallar("El campo \"" + std::string(nombre) + "\" de " + donde + " no tiene el tipo esperado.");
    }
}

// Campo obligatorio que debe ser una lista.
const json& lista(const json& j, const char* nombre, const std::string& donde) {
    const json& l = campo(j, nombre, donde);
    if (!l.is_array()) fallar("El campo \"" + std::string(nombre) + "\" de " + donde + " debe ser una lista.");
    return l;
}

const char* textoContenedor(Container c) {
    return c == Container::Mkv ? "mkv" : c == Container::Mov ? "mov" : "mp4";
}

const char* textoPreset(ResolutionPreset p) {
    switch (p) {
    case ResolutionPreset::P480: return "480p";
    case ResolutionPreset::P720: return "720p";
    case ResolutionPreset::Custom: return "custom";
    default: return "1080p";
    }
}

const char* textoEstilo(TransitionStyle e) {
    switch (e) {
    case TransitionStyle::WipeLeft: return "wipeleft";
    case TransitionStyle::WipeRight: return "wiperight";
    case TransitionStyle::SlideLeft: return "slideleft";
    case TransitionStyle::SlideRight: return "slideright";
    case TransitionStyle::Circle: return "circle";
    default: return "dissolve";
    }
}

json efectoAJson(const Effect& e) {
    if (const auto* bc = std::get_if<BrightnessContrast>(&e))
        return {{"type", "brightnessContrast"}, {"brightness", bc->brightness}, {"contrast", bc->contrast}};
    if (const auto* b = std::get_if<Blur>(&e)) return {{"type", "blur"}, {"radius", b->radius}};
    if (const auto* f = std::get_if<Fade>(&e)) return {{"type", "fade"}, {"fadeInUs", f->fadeIn}, {"fadeOutUs", f->fadeOut}};
    return {{"type", "grayscale"}};
}

Effect efectoDesdeJson(const json& j, const std::string& donde) {
    const std::string tipo = leer<std::string>(j, "type", donde);
    if (tipo == "grayscale") return Grayscale{};
    if (tipo == "brightnessContrast")
        return limitarEfecto(BrightnessContrast{leer<double>(j, "brightness", donde), leer<double>(j, "contrast", donde)});
    if (tipo == "blur") return limitarEfecto(Blur{leer<double>(j, "radius", donde)});
    if (tipo == "fade") return limitarEfecto(Fade{leer<TimeUs>(j, "fadeInUs", donde), leer<TimeUs>(j, "fadeOutUs", donde)});
    fallar("Tipo de efecto desconocido \"" + tipo + "\" en " + donde + ".");
}

} // namespace

std::string ProjectSerializer::toJson(const Project& p) {
    json j;
    j["formatVersion"] = kFormatVersion;
    j["canvas"] = {{"width", p.canvas().width}, {"height", p.canvas().height},
                   {"fps", {{"num", p.canvas().fps.num}, {"den", p.canvas().fps.den}}}};

    json medios = json::array();
    for (const auto& [id, m] : p.media().items()) {
        medios.push_back({{"id", id},
                          {"path", m.path},
                          {"durationUs", m.duration},
                          {"hasVideo", m.hasVideo},
                          {"width", m.width},
                          {"height", m.height},
                          {"fps", {{"num", m.fps.num}, {"den", m.fps.den}}},
                          {"videoCodec", m.videoCodec},
                          {"hasAudio", m.hasAudio},
                          {"sampleRate", m.sampleRate},
                          {"channels", m.channels},
                          {"audioCodec", m.audioCodec}});
    }
    j["media"] = medios;

    json pistas = json::array();
    for (const Track& t : p.timeline().tracks) {
        json clips = json::array();
        for (const Clip& c : t.clips) {
            json efectos = json::array();
            for (const Effect& e : c.effects) efectos.push_back(efectoAJson(e));
            clips.push_back({{"id", c.id},
                             {"mediaId", c.mediaId},
                             {"sourceInUs", c.sourceIn},
                             {"sourceOutUs", c.sourceOut},
                             {"startUs", c.timelineStart},
                             {"linkedClipId", c.linkedClipId},
                             {"effects", efectos},
                             {"pip",
                              {{"x", c.transform.x},
                               {"y", c.transform.y},
                               {"scale", c.transform.scale},
                               {"opacity", c.transform.opacity}}}});
        }
        json transiciones = json::array();
        for (const Transition& tr : t.transitions)
            transiciones.push_back({{"fromClipId", tr.fromClipId},
                                    {"toClipId", tr.toClipId},
                                    {"style", textoEstilo(tr.style)},
                                    {"durationUs", tr.duration}});
        pistas.push_back({{"id", t.id},
                          {"kind", t.kind == TrackKind::Video ? "video" : "audio"},
                          {"name", t.name},
                          {"muted", t.muted},
                          {"videoTrackId", t.videoTrackId},
                          {"clips", clips},
                          {"transitions", transiciones}});
    }
    j["tracks"] = pistas;

    const ExportSettings& e = p.exportSettings();
    j["export"] = {{"container", textoContenedor(e.container)},
                   {"videoCodec", e.videoCodec},
                   {"audioCodec", e.audioCodec},
                   {"videoBitrateKbps", e.videoBitrateKbps},
                   {"audioBitrateKbps", e.audioBitrateKbps},
                   {"preset", textoPreset(e.preset)},
                   {"width", e.width},
                   {"height", e.height},
                   {"outputPath", e.outputPath}};
    return j.dump(2);
}

bool ProjectSerializer::fromJson(const std::string& texto, Project& salida, std::string* error) {
    try {
        json j;
        try {
            j = json::parse(texto);
        } catch (const json::parse_error& e) {
            fallar(std::string("El archivo no es un JSON válido (") + e.what() + ").");
        }
        if (!j.is_object()) fallar("El archivo no contiene un proyecto.");
        const int version = leer<int>(j, "formatVersion", "el proyecto");
        if (version != 1 && version != kFormatVersion)
            fallar("Versión de formato no soportada: " + std::to_string(version) + " (se esperaba " +
                   std::to_string(kFormatVersion) + ").");

        Project p;
        p.timeline_.tracks.clear();
        std::set<Id> ids;  // todos los ids deben ser únicos
        auto idNuevo = [&](Id id, const std::string& donde) {
            if (id == kSinId || !ids.insert(id).second) fallar("Id repetido o inválido en " + donde + ".");
            return id;
        };

        // Lienzo.
        const json& lienzo = campo(j, "canvas", "el proyecto");
        p.canvas_.width = leer<int>(lienzo, "width", "el lienzo");
        p.canvas_.height = leer<int>(lienzo, "height", "el lienzo");
        const json& fps = campo(lienzo, "fps", "el lienzo");
        p.canvas_.fps = {leer<int>(fps, "num", "los fps"), leer<int>(fps, "den", "los fps")};
        if (p.canvas_.width < 16 || p.canvas_.height < 16 || p.canvas_.width > 7680 || p.canvas_.height > 7680 ||
            !p.canvas_.fps.valido())
            fallar("La resolución o los fps del lienzo no son válidos.");

        // Medios.
        for (const json& m : lista(j, "media", "el proyecto")) {
            MediaInfo info;
            const Id id = idNuevo(leer<Id>(m, "id", "un medio"), "los medios");
            info.path = leer<std::string>(m, "path", "un medio");
            info.duration = leer<TimeUs>(m, "durationUs", info.path);
            info.hasVideo = leer<bool>(m, "hasVideo", info.path);
            info.width = leer<int>(m, "width", info.path);
            info.height = leer<int>(m, "height", info.path);
            const json& mf = campo(m, "fps", info.path);
            info.fps = {leer<int>(mf, "num", info.path), leer<int>(mf, "den", info.path)};
            info.videoCodec = leer<std::string>(m, "videoCodec", info.path);
            info.hasAudio = leer<bool>(m, "hasAudio", info.path);
            info.sampleRate = leer<int>(m, "sampleRate", info.path);
            info.channels = leer<int>(m, "channels", info.path);
            info.audioCodec = leer<std::string>(m, "audioCodec", info.path);
            if (info.path.empty() || info.duration <= 0) fallar("Un medio no tiene ruta o duración.");
            p.media_.add(id, std::move(info));
        }

        // Pistas, clips y transiciones.
        for (const json& jt : lista(j, "tracks", "el proyecto")) {
            Track t;
            t.id = idNuevo(leer<Id>(jt, "id", "una pista"), "las pistas");
            const std::string tipo = leer<std::string>(jt, "kind", "una pista");
            if (tipo != "video" && tipo != "audio") fallar("Tipo de pista desconocido \"" + tipo + "\".");
            t.kind = (tipo == "video") ? TrackKind::Video : TrackKind::Audio;
            t.name = leer<std::string>(jt, "name", "una pista");
            t.muted = leer<bool>(jt, "muted", t.name);
            t.videoTrackId = leer<Id>(jt, "videoTrackId", t.name);
            for (const json& jc : lista(jt, "clips", t.name)) {
                Clip c;
                const std::string donde = "un clip de " + t.name;
                c.id = idNuevo(leer<Id>(jc, "id", donde), "los clips");
                c.mediaId = leer<Id>(jc, "mediaId", donde);
                c.sourceIn = leer<TimeUs>(jc, "sourceInUs", donde);
                c.sourceOut = leer<TimeUs>(jc, "sourceOutUs", donde);
                c.timelineStart = leer<TimeUs>(jc, "startUs", donde);
                c.linkedClipId = leer<Id>(jc, "linkedClipId", donde);
                for (const json& je : lista(jc, "effects", donde)) c.effects.push_back(efectoDesdeJson(je, donde));
                const json& pip = campo(jc, "pip", donde);
                c.transform = {leer<double>(pip, "x", donde), leer<double>(pip, "y", donde),
                               leer<double>(pip, "scale", donde), leer<double>(pip, "opacity", donde)};
                const MediaInfo* m = p.media_.find(c.mediaId);
                if (!m) fallar("Un clip de " + t.name + " usa un medio que no está en el proyecto.");
                if (c.sourceIn < 0 || c.sourceIn >= c.sourceOut || c.sourceOut > m->duration || c.timelineStart < 0)
                    fallar("Un clip de " + t.name + " tiene un tramo o una posición inválidos.");
                if ((t.kind == TrackKind::Video && !m->hasVideo) || (t.kind == TrackKind::Audio && !m->hasAudio))
                    fallar("Un clip de " + t.name + " usa un medio sin " + (t.kind == TrackKind::Video ? "video." : "audio."));
                for (const Effect& e : c.effects) {
                    const Fade* f = std::get_if<Fade>(&e);
                    if (f && f->fadeIn + f->fadeOut > c.duration()) fallar("Un fade de " + t.name + " supera la duración del clip.");
                }
                t.clips.push_back(std::move(c));
            }
            for (const json& jr : lista(jt, "transitions", t.name)) {
                Transition tr;
                tr.fromClipId = leer<Id>(jr, "fromClipId", "una transición");
                tr.toClipId = leer<Id>(jr, "toClipId", "una transición");
                tr.duration = leer<TimeUs>(jr, "durationUs", "una transición");
                const std::string estilo = leer<std::string>(jr, "style", "una transición");
                const std::pair<const char*, TransitionStyle> estilos[] = {
                    {"dissolve", TransitionStyle::Dissolve},   {"wipeleft", TransitionStyle::WipeLeft},
                    {"wiperight", TransitionStyle::WipeRight}, {"slideleft", TransitionStyle::SlideLeft},
                    {"slideright", TransitionStyle::SlideRight}, {"circle", TransitionStyle::Circle}};
                bool conocido = false;
                for (const auto& [nombre, valor] : estilos) {
                    if (estilo == nombre) {
                        tr.style = valor;
                        conocido = true;
                    }
                }
                if (!conocido) fallar("Estilo de transición desconocido \"" + estilo + "\".");
                t.transitions.push_back(tr);
            }
            p.timeline_.tracks.push_back(std::move(t));
        }

        // Pistas: al menos una de video, y cada una con exactamente una pista de audio.
        const Timeline& tl = p.timeline_;
        if (tl.countTracks(TrackKind::Video) == 0) fallar("El proyecto no tiene ninguna pista de video.");
        if (version == 1) {
            for (Track& t : p.timeline_.tracks) {
                if (t.kind != TrackKind::Video) continue;
                for (Clip& c : t.clips) c.transform = PipTransform{};
                break;  // solo V1, la primera de video
            }
        }
        for (const Track& t : tl.tracks) {
            if (t.kind == TrackKind::Video) {
                int deVideo = 0;
                for (const Track& a : tl.tracks) deVideo += (a.kind == TrackKind::Audio && a.videoTrackId == t.id);
                if (deVideo != 1) fallar("La pista " + t.name + " debe tener exactamente una pista de audio.");
                if (t.videoTrackId != kSinId) fallar("La pista de video " + t.name + " no puede pertenecer a otra.");
            } else if (t.videoTrackId != kSinId) {
                const Track* duena = tl.findTrack(t.videoTrackId);
                if (!duena || duena->kind != TrackKind::Video)
                    fallar("La pista " + t.name + " pertenece a una pista de video que no existe.");
            }
        }

        // Vínculos: recíprocos, mismo medio, tramo y posición, y el audio en la pista de
        // audio de la pista de video del otro.
        for (const Track& t : tl.tracks) {
            for (const Clip& c : t.clips) {
                if (!c.vinculado()) continue;
                const Track* pistaOtro = nullptr;
                const Clip* otro = tl.findClip(c.linkedClipId, &pistaOtro);
                if (!otro || otro->linkedClipId != c.id || otro->mediaId != c.mediaId || otro->sourceIn != c.sourceIn ||
                    otro->sourceOut != c.sourceOut || otro->timelineStart != c.timelineStart || pistaOtro->kind == t.kind)
                    fallar("Hay un vínculo entre clips que no es coherente (pista " + t.name + ").");
                const Track& pistaVideo = (t.kind == TrackKind::Video) ? t : *pistaOtro;
                const Track& pistaAudio = (t.kind == TrackKind::Video) ? *pistaOtro : t;
                if (pistaAudio.videoTrackId != pistaVideo.id)
                    fallar("El audio vinculado de un clip de " + pistaVideo.name + " no está en su pista de audio.");
            }
        }

        // Transiciones y solapes: la misma validación que después de cada edición. Si
        // alguna transición no corresponde a sus clips, el archivo no es coherente.
        size_t transiciones = 0;
        for (const Track& t : tl.tracks) transiciones += t.transitions.size();
        Timeline copia = tl;
        if (const Track* mala = Project::primeraPistaInvalida(copia))
            fallar("Los clips de la pista " + mala->name + " se solapan sin una transición válida.");
        size_t validas = 0;
        for (const Track& t : copia.tracks) validas += t.transitions.size();
        if (validas != transiciones) fallar("Hay transiciones que no corresponden a dos clips seguidos que se solapan.");
        for (size_t i = 0; i < copia.tracks.size(); ++i) {
            for (const Transition& tr : tl.tracks[i].transitions) {
                bool igual = false;
                for (const Transition& v : copia.tracks[i].transitions)
                    igual = igual || (v.fromClipId == tr.fromClipId && v.duration == tr.duration);
                if (!igual) fallar("La duración de una transición no coincide con el solape de sus clips.");
            }
        }
        p.timeline_ = std::move(copia);  // (ordenado)

        // Configuración de exportación.
        const json& je = campo(j, "export", "el proyecto");
        ExportSettings& e = p.export_;
        const std::string contenedor = leer<std::string>(je, "container", "la exportación");
        e.container = contenedor == "mkv" ? Container::Mkv : contenedor == "mov" ? Container::Mov : Container::Mp4;
        e.videoCodec = leer<std::string>(je, "videoCodec", "la exportación");
        e.audioCodec = leer<std::string>(je, "audioCodec", "la exportación");
        e.videoBitrateKbps = leer<int>(je, "videoBitrateKbps", "la exportación");
        e.audioBitrateKbps = leer<int>(je, "audioBitrateKbps", "la exportación");
        const std::string preset = leer<std::string>(je, "preset", "la exportación");
        e.preset = preset == "480p"     ? ResolutionPreset::P480
                   : preset == "720p"   ? ResolutionPreset::P720
                   : preset == "custom" ? ResolutionPreset::Custom
                                        : ResolutionPreset::P1080;
        e.width = leer<int>(je, "width", "la exportación");
        e.height = leer<int>(je, "height", "la exportación");
        e.outputPath = leer<std::string>(je, "outputPath", "la exportación");

        p.nextId_ = ids.empty() ? 1 : *ids.rbegin() + 1;
        p.revision_ = 0;
        p.dirty_ = false;
        salida = std::move(p);
        return true;
    } catch (const ErrorFormato& e) {
        if (error) *error = e.what();
        return false;
    } catch (const json::exception& e) {
        if (error) *error = std::string("Error de formato en el proyecto: ") + e.what();
        return false;
    }
}

} // namespace model
