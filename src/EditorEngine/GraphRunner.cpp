// GraphRunner.cpp
// Capa: ENGINE. Ver GraphRunner.h.
#include "EditorEngine/GraphRunner.h"
#include "EditorEngine/FFmpegError.h"

namespace engine {

namespace {

constexpr AVRational kMicros{1, AV_TIME_BASE};

// Argumentos del filtro `buffer` (entrada de video) a partir del decoder.
std::string argumentosVideo(const MediaDecoder& dec) {
    const AVCodecContext* c = dec.codec();
    AVRational sar = c->sample_aspect_ratio;
    if (sar.num <= 0 || sar.den <= 0) sar = {1, 1};
    AVRational fr = dec.frameRate();
    if (fr.num <= 0 || fr.den <= 0) fr = {25, 1};
    char buf[256];
    std::snprintf(buf, sizeof(buf), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d",
                  c->width, c->height, static_cast<int>(c->pix_fmt), dec.timeBase().num, dec.timeBase().den, sar.num,
                  sar.den, fr.num, fr.den);
    return buf;
}

// Argumentos del filtro `abuffer` (entrada de audio).
std::string argumentosAudio(const MediaDecoder& dec) {
    const AVCodecContext* c = dec.codec();
    char layout[128] = {};
    av_channel_layout_describe(&c->ch_layout, layout, sizeof(layout));
    char buf[384];
    std::snprintf(buf, sizeof(buf), "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
                  dec.timeBase().num, dec.timeBase().den, c->sample_rate, av_get_sample_fmt_name(c->sample_fmt), layout);
    return buf;
}

} // namespace

GraphRunner::~GraphRunner() {
    avfilter_graph_free(&grafo_);  // libera también los filtros (fuentes y sinks)
    av_frame_free(&temporal_);
}

bool GraphRunner::build(const GraphDescription& d, std::string* error) {
    auto fallar = [&](const std::string& msg) {
        error_ = msg;
        if (error) *error = msg;
        return false;
    };
    if (d.vacio) return fallar("El timeline está vacío.");

    grafo_ = avfilter_graph_alloc();
    temporal_ = av_frame_alloc();
    if (!grafo_ || !temporal_) return fallar("Sin memoria para el grafo de filtros.");
    grafo_->nb_threads = 0;  // 0 = FFmpeg elige la cantidad de hilos para los filtros

    // 1. Una fuente (`buffer`/`abuffer`) por entrada, conectada a su decoder.
    AVFilterInOut* salidas = nullptr;  // lado "salida" de las fuentes: [in0], [in1]...
    for (const GraphInput& in : d.inputs) {
        Entrada e;
        e.decoder = std::make_unique<MediaDecoder>();
        std::string err;
        const auto tipo = in.video ? MediaDecoder::Tipo::Video : MediaDecoder::Tipo::Audio;
        if (!e.decoder->open(in.mediaPath, tipo, &err) || !e.decoder->setRange(in.sourceIn, in.sourceOut, &err)) {
            avfilter_inout_free(&salidas);
            return fallar(err);
        }
        const std::string args = in.video ? argumentosVideo(*e.decoder) : argumentosAudio(*e.decoder);
        const AVFilter* filtro = avfilter_get_by_name(in.video ? "buffer" : "abuffer");
        int r = avfilter_graph_create_filter(&e.fuente, filtro, in.label.c_str(), args.c_str(), nullptr, grafo_);
        if (r < 0) {
            avfilter_inout_free(&salidas);
            return fallar("No se pudo crear la entrada " + in.label + ": " + mensajeErrorFFmpeg(r));
        }
        AVFilterInOut* io = avfilter_inout_alloc();
        io->name = av_strdup(in.label.c_str());
        io->filter_ctx = e.fuente;
        io->pad_idx = 0;
        io->next = salidas;
        salidas = io;
        entradas_.push_back(std::move(e));
    }

    // 2. Los sinks [vout] y [aout].
    int r = avfilter_graph_create_filter(&sinkVideo_, avfilter_get_by_name("buffersink"), "vout", nullptr, nullptr, grafo_);
    if (r >= 0)
        r = avfilter_graph_create_filter(&sinkAudio_, avfilter_get_by_name("abuffersink"), "aout", nullptr, nullptr, grafo_);
    if (r < 0) {
        avfilter_inout_free(&salidas);
        return fallar("No se pudieron crear las salidas del grafo: " + mensajeErrorFFmpeg(r));
    }
    AVFilterInOut* entradasSinks = avfilter_inout_alloc();
    entradasSinks->name = av_strdup("vout");
    entradasSinks->filter_ctx = sinkVideo_;
    entradasSinks->next = avfilter_inout_alloc();
    entradasSinks->next->name = av_strdup("aout");
    entradasSinks->next->filter_ctx = sinkAudio_;

    // 3. La cadena de texto conecta todo.
    r = avfilter_graph_parse_ptr(grafo_, d.filtros.c_str(), &entradasSinks, &salidas, nullptr);
    avfilter_inout_free(&entradasSinks);
    avfilter_inout_free(&salidas);
    if (r < 0) return fallar("Error en la cadena de filtros: " + mensajeErrorFFmpeg(r));

    r = avfilter_graph_config(grafo_, nullptr);
    if (r < 0) return fallar("No se pudo configurar el grafo de filtros: " + mensajeErrorFFmpeg(r));

    // El audio se entrega en bloques de tamaño fijo (cómodo para la salida de audio).
    av_buffersink_set_frame_size(sinkAudio_, bloqueAudio_);
    return true;
}

int GraphRunner::alimentar() {
    // La entrada con más pedidos sin atender es la que bloquea la salida.
    Entrada* elegida = nullptr;
    unsigned maxPedidos = 0;
    for (Entrada& e : entradas_) {
        if (e.fin) continue;
        const unsigned pedidos = av_buffersrc_get_nb_failed_requests(e.fuente);
        if (pedidos > maxPedidos) {
            maxPedidos = pedidos;
            elegida = &e;
        }
    }
    if (!elegida) {
        error_ = "El grafo de filtros no puede avanzar (ninguna entrada pide datos).";
        return AVERROR_BUG;
    }

    const int r = elegida->decoder->nextFrame(temporal_);
    if (r > 0) return av_buffersrc_add_frame(elegida->fuente, temporal_);  // el grafo toma el cuadro
    if (r == 0) {
        elegida->fin = true;
        return av_buffersrc_add_frame(elegida->fuente, nullptr);  // nullptr = fin de esta entrada
    }
    error_ = "Error al decodificar: " + mensajeErrorFFmpeg(r);
    return r;
}

int GraphRunner::pull(AVFilterContext* sink, AVFrame* out, model::TimeUs* pts) {
    if (!grafo_) return AVERROR(EINVAL);
    for (;;) {
        const int r = av_buffersink_get_frame(sink, out);
        if (r >= 0) {
            if (pts) *pts = av_rescale_q(out->pts, av_buffersink_get_time_base(sink), kMicros);
            return 1;
        }
        if (r == AVERROR_EOF) return 0;
        if (r != AVERROR(EAGAIN)) {
            error_ = "Error en el grafo de filtros: " + mensajeErrorFFmpeg(r);
            return r;
        }
        const int a = alimentar();
        if (a < 0) return a;
    }
}

int GraphRunner::pullVideo(AVFrame* out, model::TimeUs* pts) {
    return pull(sinkVideo_, out, pts);
}

int GraphRunner::pullAudio(AVFrame* out, model::TimeUs* pts) {
    return pull(sinkAudio_, out, pts);
}

} // namespace engine
