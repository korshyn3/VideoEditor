// FilterGraphBuilder.cpp
// Capa: ENGINE. Ver FilterGraphBuilder.h.
#include "EditorEngine/FilterGraphBuilder.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace engine {

namespace {

using model::TimeUs;
constexpr long long kUs = model::kMicrosPorSegundo;

// Segundos con 6 decimales (precisión de microsegundo), el formato que entiende FFmpeg.
std::string seg(TimeUs t) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%lld.%06lld", static_cast<long long>(t / kUs), static_cast<long long>(t % kUs));
    return buf;
}

// Número con 4 decimales (siempre con punto decimal).
std::string num(double x) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.4f", x);
    return buf;
}

std::string racional(model::Rational r) {
    return std::to_string(r.num) + "/" + std::to_string(r.den);
}

// Índice de cuadro REDONDEADO (para los límites de los clips).
long long cuadroRedondeado(TimeUs t, model::Rational fps) {
    const long long den = static_cast<long long>(fps.den) * kUs;
    return (t * fps.num * 2 + den) / (2 * den);
}

const model::Transition* transicionEntre(const model::Track& t, model::Id from, model::Id to) {
    for (const model::Transition& tr : t.transitions)
        if (tr.fromClipId == from && tr.toClipId == to) return &tr;
    return nullptr;
}

// Un tramo de una pista: un hueco, un clip (o una parte) o una CADENA de clips unidos
// por transiciones (que se renderiza con xfade/acrossfade y cuenta como un solo tramo).
struct Segmento {
    std::vector<const model::Clip*> clips;            // vacío = hueco
    std::vector<const model::Transition*> transiciones; // [k] une clips[k] con clips[k + 1]
    long long cuadroIni = 0, cuadroFin = 0;  // video: [ini, fin) en la rejilla de cuadros
    TimeUs ini = 0, fin = 0;                 // audio: tiempos exactos del timeline
    bool hueco() const { return clips.empty(); }
};

// Divide la pista en segmentos (huecos, clips y cadenas) que cubren exactamente [t0, fin).
// t0 no debe caer dentro de una transición (ver build).
std::vector<Segmento> segmentar(const model::Track& pista, TimeUs t0, TimeUs fin, model::Rational fps) {
    std::vector<Segmento> segs;
    long long cursorCuadro = FilterGraphBuilder::cuadroEn(t0, fps);
    const long long cuadroFinal = std::max(cursorCuadro + 1, cuadroRedondeado(fin, fps));
    TimeUs cursor = t0;

    auto hueco = [&](long long hastaCuadro, TimeUs hastaT) {
        if (hastaCuadro > cursorCuadro || hastaT > cursor) {
            Segmento h;
            h.cuadroIni = cursorCuadro;
            h.cuadroFin = std::max(cursorCuadro, hastaCuadro);
            h.ini = cursor;
            h.fin = std::max(cursor, hastaT);
            segs.push_back(h);
        }
    };

    const auto& clips = pista.clips;  // ya están ordenados por inicio
    for (size_t i = 0; i < clips.size();) {
        // Cadena: el clip i y los siguientes unidos por una transición de al menos un cuadro.
        Segmento s;
        s.clips.push_back(&clips[i]);
        size_t j = i + 1;
        for (; j < clips.size(); ++j) {
            const model::Transition* tr = transicionEntre(pista, clips[j - 1].id, clips[j].id);
            if (!tr || cuadroRedondeado(clips[j - 1].timelineEnd(), fps) <= cuadroRedondeado(clips[j].timelineStart, fps))
                break;
            s.transiciones.push_back(tr);
            s.clips.push_back(&clips[j]);
        }
        i = j;

        const model::Clip& primero = *s.clips.front();
        const model::Clip& ultimo = *s.clips.back();
        const TimeUs ini = std::max(primero.timelineStart, cursor);
        const TimeUs finTramo = std::min(ultimo.timelineEnd(), fin);
        if (finTramo <= ini) continue;  // fuera del rango pedido
        const long long ci = std::max(cuadroRedondeado(primero.timelineStart, fps), cursorCuadro);
        const long long cf = std::min(cuadroRedondeado(ultimo.timelineEnd(), fps), cuadroFinal);
        if (cf <= ci) continue;  // dura menos de medio cuadro en este rango

        // Se quitan de la cadena los clips que quedan enteros fuera del rango.
        while (s.clips.size() > 1 &&
               (s.clips.front()->timelineEnd() <= ini || cuadroRedondeado(s.clips.front()->timelineEnd(), fps) <= ci)) {
            s.clips.erase(s.clips.begin());
            s.transiciones.erase(s.transiciones.begin());
        }
        while (s.clips.size() > 1 &&
               (s.clips.back()->timelineStart >= finTramo || cuadroRedondeado(s.clips.back()->timelineStart, fps) >= cf)) {
            s.clips.pop_back();
            s.transiciones.pop_back();
        }

        hueco(ci, ini);
        s.cuadroIni = ci;
        s.cuadroFin = cf;
        s.ini = ini;
        s.fin = finTramo;
        segs.push_back(s);
        cursorCuadro = cf;
        cursor = finTramo;
    }
    hueco(cuadroFinal, fin);
    return segs;
}

// Filtros de los efectos del clip, en orden (cada uno termina en ','). Se aplican con
// pts relativos al INICIO DEL CLIP (no del segmento), así un fade empieza en el clip
// aunque el segmento arranque a la mitad.
std::string cadenaEfectos(const model::Clip& c) {
    std::string f;
    for (const model::Effect& e : c.effects) {
        if (std::holds_alternative<model::Grayscale>(e)) {
            f += "hue=s=0,";
        } else if (const auto* bc = std::get_if<model::BrightnessContrast>(&e)) {
            // y' = (y - 128)·C + 128 + B·255, escrito como y·C + K (eq no está en esta build).
            const double k = 128.0 + bc->brightness * 255.0 - 128.0 * bc->contrast;
            f += "lutyuv=y='clip(val*" + num(bc->contrast) + "+" + num(k) + ",0,255)',";
        } else if (const auto* b = std::get_if<model::Blur>(&e)) {
            if (b->radius > 0) f += "gblur=sigma=" + num(b->radius) + ",";  // boxblur no está
        } else if (const auto* fd = std::get_if<model::Fade>(&e)) {
            if (fd->fadeIn > 0) f += "fade=t=in:st=0:d=" + seg(fd->fadeIn) + ",";
            if (fd->fadeOut > 0)
                f += "fade=t=out:st=" + seg(c.duration() - fd->fadeOut) + ":d=" + seg(fd->fadeOut) + ",";
        }
    }
    return f;
}

// Cadena de un segmento de video de un clip: normaliza a la resolución/fps del proyecto,
// aplica los efectos y garantiza exactamente `n` cuadros. `desdeInicioClip` es cuánto
// del clip quedó antes del segmento (arranque a la mitad del clip).
std::string cadenaVideoClip(const RenderSettings& s, long long n, const model::Clip& clip, TimeUs desdeInicioClip) {
    const std::string W = std::to_string(s.width), H = std::to_string(s.height);
    const std::string efectos = cadenaEfectos(clip);
    // Con efectos, los pts arrancan en el tiempo del clip; el setpts del final los vuelve a 0.
    const std::string inicio = (!efectos.empty() && desdeInicioClip > 0)
                                   ? "setpts=PTS-STARTPTS+" + seg(desdeInicioClip) + "/TB,"
                                   : "setpts=PTS-STARTPTS,";
    return inicio +
           "scale=" + W + ":" + H + ":force_original_aspect_ratio=decrease:force_divisible_by=2,"
           "pad=" + W + ":" + H + ":(ow-iw)/2:(oh-ih)/2:color=black,setsar=1,"
           "fps=" + racional(s.fps) + ",format=yuv420p," + efectos +
           "tpad=stop_mode=clone:stop_duration=1,trim=end_frame=" + std::to_string(n) + ",setpts=PTS-STARTPTS";
}

std::string cadenaHuecoVideo(const RenderSettings& s, long long n) {
    return "color=c=black:s=" + std::to_string(s.width) + "x" + std::to_string(s.height) + ":r=" + racional(s.fps) +
           ",format=yuv420p,setsar=1,trim=end_frame=" + std::to_string(n);
}

// Hueco de una pista superior: transparente (deja ver las pistas de abajo).
std::string cadenaHuecoTransparente(const RenderSettings& s, long long n) {
    return "color=c=black@0.0:s=" + std::to_string(s.width) + "x" + std::to_string(s.height) + ":r=" +
           racional(s.fps) + ",format=yuva420p,setsar=1,trim=end_frame=" + std::to_string(n);
}

// Lado del recuadro de un clip con transformación: su fracción del lado del lienzo (par).
int ladoRecuadro(double scale, int ladoLienzo) {
    return std::max(2, static_cast<int>(std::lround(scale * ladoLienzo)) & ~1);
}

// Cadena de un clip con transformación (PiP): se ajusta a su recuadro igual que un clip a
// pantalla completa se ajusta al lienzo, pero con las bandas transparentes; se le aplican
// los efectos y se le agrega el canal alfa con su opacidad. Después se coloca sobre el
// lienzo con overlay (que recorta lo que queda fuera).
std::string cadenaVideoPip(const RenderSettings& s, long long n, const model::Clip& clip, TimeUs desdeInicioClip) {
    const std::string W = std::to_string(ladoRecuadro(clip.transform.scale, s.width));
    const std::string H = std::to_string(ladoRecuadro(clip.transform.scale, s.height));
    const std::string efectos = cadenaEfectos(clip);
    const std::string inicio = (!efectos.empty() && desdeInicioClip > 0)
                                   ? "setpts=PTS-STARTPTS+" + seg(desdeInicioClip) + "/TB,"
                                   : "setpts=PTS-STARTPTS,";
    std::string f = inicio + "scale=" + W + ":" + H + ":force_original_aspect_ratio=decrease:force_divisible_by=2,"
                    "setsar=1,fps=" + racional(s.fps) + ",format=yuv420p," + efectos +
                    "tpad=stop_mode=clone:stop_duration=1,trim=end_frame=" + std::to_string(n) +
                    ",setpts=PTS-STARTPTS,format=yuva420p,"
                    "pad=" + W + ":" + H + ":(ow-iw)/2:(oh-ih)/2:color=black@0.0";
    // (colorchannelmixer solo trabaja en RGB; lutyuv cambia el alfa sin convertir.)
    if (clip.transform.opacity < 1.0) f += ",lutyuv=a='val*" + num(clip.transform.opacity) + "'";
    return f;
}

// Estilo del modelo -> transición de xfade (design.md, D4).
const char* estiloXfade(model::TransitionStyle e) {
    switch (e) {
    case model::TransitionStyle::WipeLeft: return "wipeleft";
    case model::TransitionStyle::WipeRight: return "wiperight";
    case model::TransitionStyle::SlideLeft: return "slideleft";
    case model::TransitionStyle::SlideRight: return "slideright";
    case model::TransitionStyle::Circle: return "circleopen";
    default: return "fade";
    }
}

const char* kFormatoAudio = "aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo";

std::string cadenaSilencio(TimeUs dur) {
    return std::string("anullsrc=r=48000:cl=stereo,") + kFormatoAudio + ",atrim=duration=" + seg(dur);
}

// Fundidos de un audio vinculado cuyo video tiene una transición que el audio no tiene
// (el otro clip no tiene audio): el audio se desvanece (o aparece) durante la transición.
void fadesPorTransicion(const model::Timeline& tl, const model::Track& pistaAudio, const model::Clip& audio,
                        TimeUs& fadeIn, TimeUs& fadeOut) {
    fadeIn = fadeOut = 0;
    if (!audio.vinculado()) return;
    const model::Track* pistaVideo = nullptr;
    const model::Clip* video = tl.findClip(audio.linkedClipId, &pistaVideo);
    if (!video || pistaVideo->kind != model::TrackKind::Video) return;
    auto tieneTransicion = [&](bool salida) {
        for (const model::Transition& tr : pistaAudio.transitions)
            if ((salida ? tr.fromClipId : tr.toClipId) == audio.id) return true;
        return false;
    };
    for (const model::Transition& tr : pistaVideo->transitions) {
        if (tr.fromClipId == video->id && !tieneTransicion(true)) fadeOut = tr.duration;
        if (tr.toClipId == video->id && !tieneTransicion(false)) fadeIn = tr.duration;
    }
}

} // namespace

long long FilterGraphBuilder::cuadroEn(TimeUs t, model::Rational fps) {
    return (t * fps.num) / (static_cast<long long>(fps.den) * kUs);
}

TimeUs FilterGraphBuilder::tiempoDeCuadro(long long n, model::Rational fps) {
    return (n * fps.den * kUs + fps.num - 1) / fps.num;  // redondeo hacia arriba
}

RenderSettings FilterGraphBuilder::resolucionPreview(const model::CanvasSettings& canvas, int anchoMaximo) {
    RenderSettings r;
    r.fps = canvas.fps;
    r.width = std::min(canvas.width, anchoMaximo);
    r.height = static_cast<int>(static_cast<long long>(canvas.height) * r.width / canvas.width);
    r.width &= ~1;  // yuv420p necesita dimensiones pares
    r.height = std::max(2, r.height & ~1);
    return r;
}

GraphDescription FilterGraphBuilder::buildExport(const TimelineSnapshot& snap, int width, int height,
                                                 const std::string& pixFmt, const std::string& sampleFmt) {
    // El lienzo "cabe" dentro de la salida: mismo aspecto, dimensiones pares.
    const model::CanvasSettings& c = snap.canvas;
    RenderSettings lienzo{width, height, c.fps};
    if (static_cast<long long>(c.width) * height > static_cast<long long>(c.height) * width)
        lienzo.height = static_cast<int>(static_cast<long long>(width) * c.height / c.width);
    else
        lienzo.width = static_cast<int>(static_cast<long long>(height) * c.width / c.height);
    lienzo.width = std::max(2, lienzo.width & ~1);
    lienzo.height = std::max(2, lienzo.height & ~1);

    GraphDescription d = build(snap, lienzo, 0);
    if (d.vacio) return d;
    // Barras negras hasta la resolución pedida y el formato de píxel del encoder.
    const std::string finVideo = "[vout];\n";
    const size_t pv = d.filtros.rfind(finVideo);
    if (pv != std::string::npos)
        d.filtros.replace(pv, finVideo.size(),
                          "[vlienzo];\n[vlienzo]pad=" + std::to_string(width) + ":" + std::to_string(height) +
                              ":(ow-iw)/2:(oh-ih)/2:color=black,setsar=1,format=" + pixFmt + "[vout];\n");
    const std::string finAudio = "aformat=sample_fmts=flt:sample_rates=48000:channel_layouts=stereo[aout]";
    const size_t pa = d.filtros.rfind(finAudio);
    if (pa != std::string::npos)
        d.filtros.replace(pa, finAudio.size(),
                          "aformat=sample_fmts=" + sampleFmt + ":sample_rates=48000:channel_layouts=stereo[aout]");
    d.salida = {width, height, c.fps};
    return d;
}

GraphDescription FilterGraphBuilder::build(const TimelineSnapshot& snap, const RenderSettings& salida, TimeUs desde,
                                           TimeUs hasta) {
    GraphDescription d;
    d.salida = salida;
    const model::Rational fps = salida.fps;
    const TimeUs duracion = snap.timeline.duration();
    const model::Track* v1 = snap.timeline.firstTrack(model::TrackKind::Video);
    if (duracion <= 0 || !v1) return d;  // vacío

    // T0: `desde` alineado hacia abajo a la rejilla de cuadros (y dentro del timeline).
    const long long ultimoCuadro = std::max(0LL, cuadroRedondeado(duracion, fps) - 1);
    const long long cuadro0 = std::clamp(cuadroEn(std::max<TimeUs>(0, desde), fps), 0LL, ultimoCuadro);
    d.t0 = tiempoDeCuadro(cuadro0, fps);
    d.fin = (hasta < 0) ? duracion : std::clamp(hasta, d.t0 + 1, duracion);
    if (d.fin <= d.t0) d.fin = std::min(duracion, d.t0 + tiempoDeCuadro(1, fps));

    // Pistas que se renderizan: todas las de video (V1 es el fondo y cada una de las
    // siguientes se compone encima) y todas las de audio no silenciadas (de video y
    // generales; los clips de video no aportan audio, su audio es un clip vinculado).
    std::vector<const model::Track*> pistasVideo;
    for (const model::Track& t : snap.timeline.tracks)
        if (t.kind == model::TrackKind::Video) pistasVideo.push_back(&t);
    std::vector<const model::Track*> pistasAudioActivas;
    for (const model::Track& t : snap.timeline.tracks)
        if (t.kind == model::TrackKind::Audio && !t.muted) pistasAudioActivas.push_back(&t);

    // Si T0 cae DENTRO de una transición, el grafo arranca al inicio de la transición
    // (xfade/acrossfade necesitan los dos clips desde ahí) y los cuadros y muestras
    // anteriores a T0 se descartan a la salida.
    long long cuadroGrafo = cuadro0;
    for (int vuelta = 0; vuelta < 16; ++vuelta) {
        bool cambio = false;
        auto retroceder = [&](const model::Track& t, bool video) {
            for (const model::Transition& tr : t.transitions) {
                const model::Clip* from = t.findClip(tr.fromClipId);
                const model::Clip* to = t.findClip(tr.toClipId);
                if (!from || !to) continue;
                if (video) {
                    const long long ini = cuadroRedondeado(to->timelineStart, fps);
                    if (cuadroGrafo > ini && cuadroGrafo < cuadroRedondeado(from->timelineEnd(), fps)) {
                        cuadroGrafo = ini;
                        cambio = true;
                    }
                } else {
                    const TimeUs tg = tiempoDeCuadro(cuadroGrafo, fps);
                    if (tg > to->timelineStart && tg < from->timelineEnd()) {
                        cuadroGrafo = cuadroEn(to->timelineStart, fps);
                        cambio = true;
                    }
                }
            }
        };
        for (const model::Track* t : pistasVideo) retroceder(*t, true);
        for (const model::Track* t : pistasAudioActivas) retroceder(*t, false);
        if (!cambio) break;
    }
    const TimeUs tGrafo = tiempoDeCuadro(cuadroGrafo, fps);
    const long long cuadrosDescartados = cuadro0 - cuadroGrafo;
    const long long muestrasDescartadas = (d.t0 - tGrafo) * kSampleRate / kUs;

    std::string g;
    int nEntrada = 0;
    auto nuevaEntrada = [&](const model::Clip& c, bool video, TimeUs inicioTimeline, TimeUs finTimeline) {
        const model::MediaInfo* info = snap.media.find(c.mediaId);
        GraphInput in;
        in.label = "in" + std::to_string(nEntrada++);
        in.mediaPath = info ? info->path : std::string();
        in.video = video;
        // (Por el redondeo a la rejilla, el inicio puede caer un poco antes del clip.)
        in.sourceIn = std::max(c.sourceIn, c.sourceIn + (inicioTimeline - c.timelineStart));
        // Un cuadro de margen al final: trim/atrim cortan exactamente.
        in.sourceOut = std::min(c.sourceOut, c.sourceIn + (finTimeline - c.timelineStart) + tiempoDeCuadro(1, fps));
        d.inputs.push_back(in);
        return in;
    };

    // ---- Video: una pista por cada pista de video, compuestas de abajo hacia arriba ----
    // Un clip de video de `n` cuadros desde el cuadro `a`, con la etiqueta dada. En una
    // pista superior es un PiP sobre un lienzo transparente; en V1, sobre el fondo negro
    // (y sin transformación ocupa todo el lienzo, sin overlay).
    auto clipVideo = [&](const model::Clip& c, long long a, long long n, long long finEntrada,
                         const std::string& etiqueta, bool superior) {
        const TimeUs iniClip = tiempoDeCuadro(a, fps);
        // Un medio offline (no se encontró al abrir el proyecto) se ve en negro: todo el
        // lienzo en V1, el recuadro del clip en las pistas superiores.
        const model::MediaInfo* info = snap.media.find(c.mediaId);
        const bool offline = !info || info->offline;
        if (offline && !superior) {
            g += cadenaHuecoVideo(salida, n) + "[" + etiqueta + "];\n";
            return;
        }
        const long x = std::lround(c.transform.x * salida.width);
        const long y = std::lround(c.transform.y * salida.height);
        if (offline) {
            g += "color=c=black:s=" + std::to_string(ladoRecuadro(c.transform.scale, salida.width)) + "x" +
                 std::to_string(ladoRecuadro(c.transform.scale, salida.height)) + ":r=" + racional(fps) +
                 ",format=yuva420p,trim=end_frame=" + std::to_string(n) + "[" + etiqueta + "p];\n";
        } else {
            const GraphInput in = nuevaEntrada(c, true, iniClip, tiempoDeCuadro(finEntrada, fps));
            const TimeUs desde = std::max<TimeUs>(0, iniClip - c.timelineStart);
            if (!superior && c.transform == model::PipTransform{}) {
                g += "[" + in.label + "]" + cadenaVideoClip(salida, n, c, desde) + "[" + etiqueta + "];\n";
                return;
            }
            g += "[" + in.label + "]" + cadenaVideoPip(salida, n, c, desde) + "[" + etiqueta + "p];\n";
        }
        g += (superior ? cadenaHuecoTransparente(salida, n) : cadenaHuecoVideo(salida, n)) + "[" + etiqueta + "b];\n";
        // (V1 sigue en yuv420p, para que xfade y concat reciban lo mismo de todos sus clips.)
        g += "[" + etiqueta + "b][" + etiqueta + "p]overlay=x=" + std::to_string(x) + ":y=" + std::to_string(y) +
             ":eof_action=pass" + (superior ? "" : ",format=yuv420p") + "[" + etiqueta + "];\n";
    };

    // Un segmento de video con la etiqueta dada: hueco (negro en V1, transparente en las
    // superiores), un clip, o una cadena de clips unidos con xfade. Cada clip de una
    // cadena se renderiza hasta su final; la salida de xfade dura la + lb - D, y al final
    // la cadena se corta a su rango.
    auto segmentoVideo = [&](const Segmento& s, const std::string& etiqueta, bool superior) {
        const long long n = s.cuadroFin - s.cuadroIni;
        if (s.hueco()) {
            g += (superior ? cadenaHuecoTransparente(salida, n) : cadenaHuecoVideo(salida, n)) + "[" + etiqueta + "];\n";
            return;
        }
        if (s.clips.size() == 1) {
            clipVideo(*s.clips[0], s.cuadroIni, n, s.cuadroFin, etiqueta, superior);
            return;
        }
        std::string anterior;
        long long acumulado = 0;  // cuadros que lleva la salida de la cadena
        for (size_t k = 0; k < s.clips.size(); ++k) {
            const model::Clip& c = *s.clips[k];
            const long long a = (k == 0) ? s.cuadroIni : cuadroRedondeado(c.timelineStart, fps);
            const long long b = cuadroRedondeado(c.timelineEnd(), fps);
            const long long nk = std::max(1LL, b - a);
            const std::string clipEtq = etiqueta + "c" + std::to_string(k);
            clipVideo(c, a, nk, b, clipEtq, superior);
            if (k == 0) {
                anterior = clipEtq;
                acumulado = nk;
                continue;
            }
            const model::Clip& prev = *s.clips[k - 1];
            const long long dk = std::clamp(
                cuadroRedondeado(prev.timelineEnd(), fps) - cuadroRedondeado(c.timelineStart, fps), 1LL,
                std::min(acumulado, nk));
            const std::string x = etiqueta + "x" + std::to_string(k);
            g += "[" + anterior + "][" + clipEtq + "]xfade=transition=" + estiloXfade(s.transiciones[k - 1]->style) +
                 ":duration=" + seg(tiempoDeCuadro(dk, fps)) + ":offset=" + seg(tiempoDeCuadro(acumulado - dk, fps)) +
                 "[" + x + "];\n";
            anterior = x;
            acumulado += nk - dk;
        }
        g += "[" + anterior + "]tpad=stop_mode=clone:stop_duration=1,trim=end_frame=" + std::to_string(n) +
             ",setpts=PTS-STARTPTS[" + etiqueta + "];\n";
    };

    // Cada pista es un concat de sus segmentos. Una pista superior sin clips en el rango
    // no se agrega.
    std::vector<std::string> pistasV;  // cadena "[s0][s1]concat..." de cada pista que se compone
    for (size_t p = 0; p < pistasVideo.size(); ++p) {
        const bool superior = p > 0;
        const auto segs = segmentar(*pistasVideo[p], tGrafo, d.fin, fps);
        if (superior && std::all_of(segs.begin(), segs.end(), [](const Segmento& x) { return x.hueco(); })) continue;
        std::string concat;
        int n = 0;
        for (size_t i = 0; i < segs.size(); ++i) {
            if (segs[i].cuadroFin - segs[i].cuadroIni <= 0) continue;
            const std::string etiqueta = "v" + std::to_string(p + 1) + "s" + std::to_string(i);
            segmentoVideo(segs[i], etiqueta, superior);
            concat += "[" + etiqueta + "]";
            ++n;
        }
        if (n == 0) {
            if (!superior) return d;
            continue;
        }
        pistasV.push_back(concat + (n > 1 ? "concat=n=" + std::to_string(n) + ":v=1:a=0" : ""));
    }

    // Composición: V1 de fondo y cada pista superior encima con overlay. eof_action=pass:
    // si una pista superior terminara antes, el fondo sigue (no se congela).
    std::string video = pistasV[0];
    for (size_t p = 1; p < pistasV.size(); ++p) {
        const std::string fondo = "vc" + std::to_string(p - 1), arriba = "vt" + std::to_string(p);
        g += video + (video.back() == ']' ? "null" : "") + "[" + fondo + "];\n";
        g += pistasV[p] + (pistasV[p].back() == ']' ? "null" : "") + "[" + arriba + "];\n";
        video = "[" + fondo + "][" + arriba + "]overlay=x=0:y=0:eof_action=pass,format=yuv420p";
    }
    // Los pts de salida van en tiempo absoluto del timeline. round(): setpts trunca el
    // resultado, y 4.6 s / (1/25) da 114.999... Al descartar cuadros se numeran por
    // índice (N), que no depende de los pts que deja xfade.
    g += video + (video.back() == ']' ? "" : ",") +
         (cuadrosDescartados > 0 ? "trim=start_frame=" + std::to_string(cuadrosDescartados) + ",setpts=round(N*" +
                                       std::to_string(fps.den) + "/" + std::to_string(fps.num) + "/TB+"
                                 : std::string("setpts=round(PTS-STARTPTS+")) +
         seg(d.t0) + "/TB)[vout];\n";

    // ---- Audio: una pista de la mezcla por cada fuente de sonido ----
    // Cada pista es un concat de tramos (audio de un clip, silencio o una cadena de
    // clips unidos con acrossfade) desde T0 hasta el final. Las pistas que en ese rango
    // solo tendrían silencio no se agregan.
    std::vector<std::string> pistasAudio;
    auto pistaDeAudio = [&](const model::Track& pista) {
        const auto segs = segmentar(pista, tGrafo, d.fin, fps);
        const std::string prefijo = "a" + std::to_string(pistasAudio.size()) + "s";
        std::string cadena, concatA;
        int n = 0;
        bool suena = false;

        // Audio de un clip desde `ini` (del timeline) durante `dur`, con sus fundidos.
        auto audioClip = [&](const model::Clip& c, TimeUs ini, TimeUs dur, const std::string& etiqueta) {
            const model::MediaInfo* info = snap.media.find(c.mediaId);
            if (!info || !info->hasAudio || info->offline) {  // offline: silencio
                cadena += cadenaSilencio(dur) + "[" + etiqueta + "];\n";
                return;
            }
            const GraphInput in = nuevaEntrada(c, false, ini, ini + dur);
            TimeUs fadeIn = 0, fadeOut = 0;
            fadesPorTransicion(snap.timeline, pista, c, fadeIn, fadeOut);
            const TimeUs desdeInicio = std::max<TimeUs>(0, ini - c.timelineStart);
            std::string fades;
            if (fadeIn > desdeInicio) fades += "afade=t=in:st=0:d=" + seg(fadeIn) + ",";
            if (fadeOut > 0) fades += "afade=t=out:st=" + seg(c.duration() - fadeOut) + ":d=" + seg(fadeOut) + ",";
            if (!fades.empty())  // como en video: pts en el tiempo del clip para los fundidos
                fades = "asetpts=PTS-STARTPTS+" + seg(desdeInicio) + "/TB," + fades + "asetpts=PTS-STARTPTS,";
            cadena += "[" + in.label + "]atrim=start=" + seg(in.sourceIn) + ":end=" + seg(in.sourceIn + dur) +
                      ",asetpts=PTS-STARTPTS,aresample=48000," + kFormatoAudio + "," + fades +
                      "apad=whole_dur=" + seg(dur) + ",atrim=duration=" + seg(dur) + "[" + etiqueta + "];\n";
            suena = true;
        };

        for (size_t i = 0; i < segs.size(); ++i) {
            const Segmento& s = segs[i];
            const TimeUs dur = s.fin - s.ini;
            if (dur <= 0) continue;
            const std::string etiqueta = prefijo + std::to_string(i);
            if (s.hueco()) {
                cadena += cadenaSilencio(dur) + "[" + etiqueta + "];\n";
            } else if (s.clips.size() == 1) {
                audioClip(*s.clips[0], s.ini, dur, etiqueta);
            } else {
                // Cadena: cada clip hasta su final, unidos con acrossfade (salida la + lb - D).
                std::string anterior;
                TimeUs acumulado = 0;
                for (size_t k = 0; k < s.clips.size(); ++k) {
                    const model::Clip& c = *s.clips[k];
                    const TimeUs a = (k == 0) ? s.ini : c.timelineStart;
                    const TimeUs dk = c.timelineEnd() - a;
                    const std::string clipEtq = etiqueta + "c" + std::to_string(k);
                    audioClip(c, a, dk, clipEtq);
                    if (k == 0) {
                        anterior = clipEtq;
                        acumulado = dk;
                        continue;
                    }
                    const TimeUs solape = std::clamp<TimeUs>(s.clips[k - 1]->timelineEnd() - c.timelineStart, 1,
                                                             std::min(acumulado, dk));
                    const std::string x = etiqueta + "x" + std::to_string(k);
                    cadena += "[" + anterior + "][" + clipEtq + "]acrossfade=d=" + seg(solape) + "[" + x + "];\n";
                    anterior = x;
                    acumulado += dk - solape;
                }
                cadena += "[" + anterior + "]apad=whole_dur=" + seg(dur) + ",atrim=duration=" + seg(dur) + "[" +
                          etiqueta + "];\n";
            }
            concatA += "[" + etiqueta + "]";
            ++n;
        }
        if (!suena) return;
        g += cadena;
        if (n > 1) {
            const std::string salidaPista = "[at" + std::to_string(pistasAudio.size()) + "]";
            g += concatA + "concat=n=" + std::to_string(n) + ":v=0:a=1" + salidaPista + ";\n";
            pistasAudio.push_back(salidaPista);
        } else {
            pistasAudio.push_back(concatA);
        }
    };
    for (const model::Track* t : pistasAudioActivas) pistaDeAudio(*t);

    // ---- Mezcla final de audio ----
    std::string mezcla;
    for (const auto& p : pistasAudio) mezcla += p;
    if (pistasAudio.empty())
        g += cadenaSilencio(d.fin - tGrafo);
    else if (pistasAudio.size() == 1)
        g += mezcla + "anull";
    else
        g += mezcla + "amix=inputs=" + std::to_string(pistasAudio.size()) + ":duration=longest:normalize=0";
    if (muestrasDescartadas > 0)
        g += ",atrim=start_sample=" + std::to_string(muestrasDescartadas) + ",asetpts=round(N/SR/TB+";
    else
        g += ",asetpts=round(PTS-STARTPTS+";
    g += seg(d.t0) + "/TB),aformat=sample_fmts=flt:sample_rates=48000:channel_layouts=stereo[aout]";

    d.filtros = std::move(g);
    d.vacio = false;
    return d;
}

} // namespace engine
