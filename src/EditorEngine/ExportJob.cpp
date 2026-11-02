// ExportJob.cpp
// Capa: ENGINE. Ver ExportJob.h.
#include "EditorEngine/ExportJob.h"
#include "EditorEngine/EncoderCatalog.h"
#include "EditorEngine/FFmpegError.h"
#include "EditorEngine/GraphRunner.h"
#include "EditorEngine/ffmpeg_includes.h"

#include <filesystem>

namespace engine {

namespace {

constexpr AVRational kMicros{1, 1000000};

template <class T>
T primerFormato(const AVCodec* codec, AVCodecConfig config, T porDefecto) {
    const void* valores = nullptr;
    int n = 0;
    if (avcodec_get_supported_config(nullptr, codec, config, 0, &valores, &n) >= 0 && valores && n > 0)
        return static_cast<const T*>(valores)[0];
    return porDefecto;
}

std::filesystem::path rutaDesdeUtf8(const std::string& utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

// Todo lo que hay que cerrar al terminar (en el orden inverso al de apertura).
struct Salida {
    AVFormatContext* fmt = nullptr;
    AVCodecContext* video = nullptr;
    AVCodecContext* audio = nullptr;
    AVStream* streamVideo = nullptr;
    AVStream* streamAudio = nullptr;
    AVPacket* paquete = nullptr;
    AVFrame* cuadro = nullptr;
    bool archivoAbierto = false;

    ~Salida() {
        av_frame_free(&cuadro);
        av_packet_free(&paquete);
        avcodec_free_context(&video);
        avcodec_free_context(&audio);
        if (fmt) {
            if (archivoAbierto) avio_closep(&fmt->pb);
            avformat_free_context(fmt);
        }
    }
};

// Envía un cuadro (o nullptr = vaciar el encoder) y escribe los paquetes que salgan.
int codificar(Salida& s, AVCodecContext* enc, AVStream* stream, AVFrame* cuadro) {
    int r = avcodec_send_frame(enc, cuadro);
    if (r < 0) return r;
    for (;;) {
        r = avcodec_receive_packet(enc, s.paquete);
        if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) return 0;
        if (r < 0) return r;
        // Los encoders MF no ponen la duración de los paquetes de video: sin ella, el
        // muxer calcula el stream un cuadro más corto (la base de tiempo es 1/fps).
        if (s.paquete->duration <= 0 && enc->codec_type == AVMEDIA_TYPE_VIDEO) s.paquete->duration = 1;
        av_packet_rescale_ts(s.paquete, enc->time_base, stream->time_base);
        s.paquete->stream_index = stream->index;
        r = av_interleaved_write_frame(s.fmt, s.paquete);  // (toma el paquete)
        if (r < 0) return r;
    }
}

} // namespace

ExportJob::ExportJob(std::function<void()> alTerminar) : alTerminar_(std::move(alTerminar)) {}

ExportJob::~ExportJob() {
    cancel();
    if (hilo_.joinable()) hilo_.join();
}

bool ExportJob::start(ExportRequest pedido) {
    if (enCurso()) return false;
    if (hilo_.joinable()) hilo_.join();  // la exportación anterior ya terminó
    {
        std::lock_guard lock(mutex_);
        error_.clear();
    }
    progreso_ = 0.0;
    estado_ = ExportEstado::EnCurso;
    hilo_ = std::jthread([this, pedido = std::move(pedido)](std::stop_token parar) {
        const ExportResult r = exportar(pedido, parar, &progreso_);
        {
            std::lock_guard lock(mutex_);
            error_ = r.error;
        }
        estado_ = r.estado;
        if (alTerminar_) alTerminar_();
    });
    return true;
}

void ExportJob::cancel() {
    if (hilo_.joinable()) hilo_.request_stop();
}

ExportResult ExportJob::resultado() const {
    std::lock_guard lock(mutex_);
    return {estado_.load(), error_};
}

ExportResult ExportJob::exportar(const ExportRequest& p, std::stop_token parar, std::atomic<double>* progreso) {
    const std::filesystem::path ruta = rutaDesdeUtf8(p.rutaSalida);
    Salida s;
    bool archivoCreado = false;
    // Al fallar o cancelar se cierra todo y se borra el archivo parcial.
    auto terminarMal = [&](ExportEstado estado, std::string error) {
        if (s.fmt && s.archivoAbierto) {
            avio_closep(&s.fmt->pb);
            s.archivoAbierto = false;
        }
        std::error_code ec;
        if (archivoCreado) std::filesystem::remove(ruta, ec);
        return ExportResult{estado, std::move(error)};
    };
    auto fallo = [&](const std::string& paso, int r) {
        return terminarMal(ExportEstado::Error, paso + ": " + mensajeErrorFFmpeg(r));
    };

    const model::TimeUs duracion = p.snap.timeline.duration();
    if (duracion <= 0) return {ExportEstado::Error, "El timeline está vacío: no hay nada que exportar."};
    const model::Rational fps = p.snap.canvas.fps;

    // ---- 1. Encoders ----
    const AVCodec* codecV = avcodec_find_encoder_by_name(p.encoderVideo.c_str());
    const AVCodec* codecA = avcodec_find_encoder_by_name(p.encoderAudio.c_str());
    if (!codecV) return {ExportEstado::Error, "No existe el encoder de video \"" + p.encoderVideo + "\"."};
    if (!codecA) return {ExportEstado::Error, "No existe el encoder de audio \"" + p.encoderAudio + "\"."};

    int r = avformat_alloc_output_context2(&s.fmt, nullptr, EncoderCatalog::formato(p.contenedor), p.rutaSalida.c_str());
    if (r < 0 || !s.fmt) return fallo("No se pudo crear el contenedor", r);
    const bool cabeceraGlobal = (s.fmt->oformat->flags & AVFMT_GLOBALHEADER) != 0;

    s.video = avcodec_alloc_context3(codecV);
    s.video->width = p.width;
    s.video->height = p.height;
    s.video->time_base = {fps.den, fps.num};
    s.video->framerate = {fps.num, fps.den};
    s.video->sample_aspect_ratio = {1, 1};
    s.video->pix_fmt = primerFormato(codecV, AV_CODEC_CONFIG_PIX_FORMAT, AV_PIX_FMT_YUV420P);
    s.video->bit_rate = static_cast<int64_t>(p.videoKbps) * 1000;
    s.video->gop_size = 2 * fps.num / std::max(1, fps.den);  // un cuadro clave cada ~2 s
    if (p.encoderVideo.ends_with("_mf")) {
        // Los encoders MF respetan el bitrate de forma aproximada; CBR ayuda (design.md).
        av_opt_set(s.video->priv_data, "rate_control", "cbr", 0);
        s.video->rc_max_rate = s.video->bit_rate;
    }
    if (cabeceraGlobal) s.video->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    r = avcodec_open2(s.video, codecV, nullptr);
    if (r < 0) return fallo("No se pudo iniciar el encoder de video " + p.encoderVideo, r);

    s.audio = avcodec_alloc_context3(codecA);
    s.audio->sample_rate = FilterGraphBuilder::kSampleRate;
    av_channel_layout_default(&s.audio->ch_layout, 2);
    s.audio->sample_fmt = primerFormato(codecA, AV_CODEC_CONFIG_SAMPLE_FORMAT, AV_SAMPLE_FMT_FLTP);
    s.audio->time_base = {1, FilterGraphBuilder::kSampleRate};
    s.audio->bit_rate = static_cast<int64_t>(p.audioKbps) * 1000;
    if (cabeceraGlobal) s.audio->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    r = avcodec_open2(s.audio, codecA, nullptr);
    if (r < 0) return fallo("No se pudo iniciar el encoder de audio " + p.encoderAudio, r);
    const bool tamanoVariable = (codecA->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) != 0;
    const int bloque = (tamanoVariable || s.audio->frame_size <= 0) ? GraphRunner::kMuestrasPorBloque : s.audio->frame_size;

    // ---- 2. Streams y archivo ----
    s.streamVideo = avformat_new_stream(s.fmt, nullptr);
    s.streamAudio = avformat_new_stream(s.fmt, nullptr);
    if (!s.streamVideo || !s.streamAudio) return fallo("No se pudieron crear los streams", AVERROR(ENOMEM));
    avcodec_parameters_from_context(s.streamVideo->codecpar, s.video);
    avcodec_parameters_from_context(s.streamAudio->codecpar, s.audio);
    s.streamVideo->time_base = s.video->time_base;
    s.streamAudio->time_base = s.audio->time_base;

    r = avio_open(&s.fmt->pb, p.rutaSalida.c_str(), AVIO_FLAG_WRITE);
    if (r < 0) return fallo("No se pudo crear el archivo \"" + p.rutaSalida + "\"", r);
    s.archivoAbierto = true;
    archivoCreado = true;
    r = avformat_write_header(s.fmt, nullptr);
    if (r < 0) return fallo("No se pudo escribir la cabecera del contenedor", r);

    // ---- 3. El grafo del timeline (el mismo que el preview) ----
    const GraphDescription d = FilterGraphBuilder::buildExport(
        p.snap, p.width, p.height, av_get_pix_fmt_name(s.video->pix_fmt), av_get_sample_fmt_name(s.audio->sample_fmt));
    if (d.vacio) return terminarMal(ExportEstado::Error, "El timeline está vacío: no hay nada que exportar.");
    GraphRunner runner;
    runner.setTamanoBloqueAudio(bloque);
    std::string err;
    if (!runner.build(d, &err)) return terminarMal(ExportEstado::Error, err);

    // ---- 4. Codificar: siempre se avanza el stream que va más atrás (así el muxer
    // recibe los paquetes casi en orden y no acumula). ----
    s.paquete = av_packet_alloc();
    s.cuadro = av_frame_alloc();
    bool finVideo = false, finAudio = false;
    model::TimeUs tVideo = 0, tAudio = 0;
    int64_t muestras = 0;
    while (!finVideo || !finAudio) {
        if (parar.stop_requested()) return terminarMal(ExportEstado::Cancelado, "Exportación cancelada.");
        model::TimeUs pts = 0;
        if (!finVideo && (finAudio || tVideo <= tAudio)) {
            r = runner.pullVideo(s.cuadro, &pts);
            if (r < 0) return terminarMal(ExportEstado::Error, runner.ultimoError());
            if (r == 0) {
                finVideo = true;
                r = codificar(s, s.video, s.streamVideo, nullptr);
            } else {
                // pts en cuadros del proyecto (la base de tiempo del encoder es 1/fps).
                s.cuadro->pts = av_rescale_q_rnd(pts, kMicros, s.video->time_base,
                                                 static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                s.cuadro->pict_type = AV_PICTURE_TYPE_NONE;
                tVideo = pts;
                r = codificar(s, s.video, s.streamVideo, s.cuadro);
                av_frame_unref(s.cuadro);
            }
            if (r < 0) return fallo("Error al codificar el video (" + p.encoderVideo + ")", r);
        } else {
            r = runner.pullAudio(s.cuadro, &pts);
            if (r < 0) return terminarMal(ExportEstado::Error, runner.ultimoError());
            if (r == 0) {
                finAudio = true;
                r = codificar(s, s.audio, s.streamAudio, nullptr);
            } else {
                s.cuadro->pts = muestras;  // en muestras (base 1/48000), sin huecos
                muestras += s.cuadro->nb_samples;
                tAudio = pts;
                r = codificar(s, s.audio, s.streamAudio, s.cuadro);
                av_frame_unref(s.cuadro);
            }
            if (r < 0) return fallo("Error al codificar el audio (" + p.encoderAudio + ")", r);
        }
        if (progreso) *progreso = std::min(1.0, static_cast<double>(std::min(tVideo, tAudio)) / duracion);
    }

    r = av_write_trailer(s.fmt);
    if (r < 0) return fallo("No se pudo cerrar el contenedor", r);
    if (progreso) *progreso = 1.0;
    return {ExportEstado::Terminado, {}};
}

} // namespace engine
