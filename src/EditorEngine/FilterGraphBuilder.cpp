// FilterGraphBuilder.cpp
// Capa: ENGINE. Ver FilterGraphBuilder.h.
#include "EditorEngine/FilterGraphBuilder.h"

#include <algorithm>
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

// Un tramo de una pista: un clip (o una parte) o un hueco.
struct Segmento {
    const model::Clip* clip = nullptr;  // nullptr = hueco
    long long cuadroIni = 0, cuadroFin = 0;  // video: [ini, fin) en la rejilla de cuadros
    TimeUs ini = 0, fin = 0;                 // audio: tiempos exactos del timeline
};

// Divide la pista en segmentos (clips y huecos) que cubren exactamente [t0, fin).
std::vector<Segmento> segmentar(const model::Track& pista, TimeUs t0, TimeUs fin, model::Rational fps) {
    std::vector<Segmento> segs;
    long long cursorCuadro = FilterGraphBuilder::cuadroEn(t0, fps);
    const long long cuadroFinal = std::max(cursorCuadro + 1, cuadroRedondeado(fin, fps));
    TimeUs cursor = t0;

    auto hueco = [&](long long hastaCuadro, TimeUs hastaT) {
        if (hastaCuadro > cursorCuadro || hastaT > cursor)
            segs.push_back({nullptr, cursorCuadro, std::max(cursorCuadro, hastaCuadro), cursor, std::max(cursor, hastaT)});
    };

    for (const model::Clip& c : pista.clips) {  // ya están ordenados por inicio
        const TimeUs ini = std::max(c.timelineStart, cursor);
        const TimeUs finClip = std::min(c.timelineEnd(), fin);
        if (finClip <= ini) continue;  // fuera del rango pedido
        const long long ci = std::max(cuadroRedondeado(c.timelineStart, fps), cursorCuadro);
        const long long cf = std::min(cuadroRedondeado(c.timelineEnd(), fps), cuadroFinal);
        if (cf <= ci) continue;  // el clip dura menos de medio cuadro en este rango

        hueco(ci, ini);
        segs.push_back({&c, ci, cf, ini, finClip});
        cursorCuadro = cf;
        cursor = finClip;
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

const char* kFormatoAudio = "aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo";

std::string cadenaSilencio(TimeUs dur) {
    return std::string("anullsrc=r=48000:cl=stereo,") + kFormatoAudio + ",atrim=duration=" + seg(dur);
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

    // ---- Video: pista V1 (las pistas superiores se agregan en la fase 8) ----
    const auto segsV1 = segmentar(*v1, d.t0, d.fin, fps);
    std::string concatV;
    for (size_t i = 0; i < segsV1.size(); ++i) {
        const Segmento& s = segsV1[i];
        const long long n = s.cuadroFin - s.cuadroIni;
        if (n <= 0) continue;
        const std::string etiqueta = "v1s" + std::to_string(i);
        if (s.clip) {
            const TimeUs iniSeg = tiempoDeCuadro(s.cuadroIni, fps);
            const GraphInput in = nuevaEntrada(*s.clip, true, iniSeg, tiempoDeCuadro(s.cuadroFin, fps));
            const TimeUs desdeInicioClip = std::max<TimeUs>(0, iniSeg - s.clip->timelineStart);
            g += "[" + in.label + "]" + cadenaVideoClip(salida, n, *s.clip, desdeInicioClip) + "[" + etiqueta + "];\n";
        } else {
            g += cadenaHuecoVideo(salida, n) + "[" + etiqueta + "];\n";
        }
        concatV += "[" + etiqueta + "]";
    }
    const size_t nV = std::count(concatV.begin(), concatV.end(), '[');
    if (nV == 0) return d;
    g += concatV + (nV > 1 ? "concat=n=" + std::to_string(nV) + ":v=1:a=0," : "") +
         "setpts=PTS-STARTPTS+" + seg(d.t0) + "/TB[vout];\n";

    // ---- Audio: una pista de la mezcla por cada fuente de sonido ----
    // Cada pista es un concat de tramos (audio de un clip o silencio) desde T0 hasta el
    // final. Las pistas que en ese rango solo tendrían silencio no se agregan.
    std::vector<std::string> pistasAudio;
    auto pistaDeAudio = [&](const model::Track& pista) {
        const auto segs = segmentar(pista, d.t0, d.fin, fps);
        const std::string prefijo = "a" + std::to_string(pistasAudio.size()) + "s";
        std::string cadena, concatA;
        int n = 0;
        bool suena = false;
        for (size_t i = 0; i < segs.size(); ++i) {
            const Segmento& s = segs[i];
            const TimeUs dur = s.fin - s.ini;
            if (dur <= 0) continue;
            const std::string etiqueta = prefijo + std::to_string(i);
            const model::MediaInfo* info = s.clip ? snap.media.find(s.clip->mediaId) : nullptr;
            if (s.clip && info && info->hasAudio) {
                const GraphInput in = nuevaEntrada(*s.clip, false, s.ini, s.fin);
                cadena += "[" + in.label + "]atrim=start=" + seg(in.sourceIn) + ":end=" + seg(in.sourceIn + dur) +
                          ",asetpts=PTS-STARTPTS,aresample=48000," + kFormatoAudio + ",apad=whole_dur=" + seg(dur) +
                          ",atrim=duration=" + seg(dur) + "[" + etiqueta + "];\n";
                suena = true;
            } else {
                cadena += cadenaSilencio(dur) + "[" + etiqueta + "];\n";
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

    // Todo el sonido sale de las pistas de audio (de video y generales) que no están
    // silenciadas; los clips de video no aportan audio (su audio es un clip vinculado).
    for (const model::Track& t : snap.timeline.tracks)
        if (t.kind == model::TrackKind::Audio && !t.muted) pistaDeAudio(t);

    // ---- Mezcla final de audio ----
    std::string mezcla;
    for (const auto& p : pistasAudio) mezcla += p;
    if (pistasAudio.empty())
        g += cadenaSilencio(d.fin - d.t0);
    else if (pistasAudio.size() == 1)
        g += mezcla + "anull";
    else
        g += mezcla + "amix=inputs=" + std::to_string(pistasAudio.size()) + ":duration=longest:normalize=0";
    g += ",asetpts=PTS-STARTPTS+" + seg(d.t0) + "/TB,aformat=sample_fmts=flt:sample_rates=48000:channel_layouts=stereo[aout]";

    d.filtros = std::move(g);
    d.vacio = false;
    return d;
}

} // namespace engine
