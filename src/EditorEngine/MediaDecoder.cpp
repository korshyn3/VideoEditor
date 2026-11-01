// MediaDecoder.cpp
// Capa: ENGINE. Ver MediaDecoder.h.
#include "EditorEngine/MediaDecoder.h"
#include "EditorEngine/FFmpegError.h"

namespace engine {

namespace {
constexpr AVRational kMicros{1, AV_TIME_BASE};  // AV_TIME_BASE = 1'000'000 = TimeUs
}

MediaDecoder::~MediaDecoder() {
    av_packet_free(&paquete_);
    avcodec_free_context(&codec_);
    avformat_close_input(&fmt_);
}

model::TimeUs MediaDecoder::aMicros(int64_t pts) const {
    return av_rescale_q(pts, stream_->time_base, kMicros);
}

int64_t MediaDecoder::aPts(model::TimeUs t) const {
    return av_rescale_q(t, kMicros, stream_->time_base);
}

bool MediaDecoder::open(const std::string& rutaUtf8, Tipo tipo, std::string* error) {
    tipo_ = tipo;
    auto fallar = [&](const std::string& causa) {
        if (error) *error = "No se pudo abrir \"" + rutaUtf8 + "\" para decodificar: " + causa;
        return false;
    };

    int err = avformat_open_input(&fmt_, rutaUtf8.c_str(), nullptr, nullptr);
    if (err < 0) return fallar(mensajeErrorFFmpeg(err));
    err = avformat_find_stream_info(fmt_, nullptr);
    if (err < 0) return fallar(mensajeErrorFFmpeg(err));

    // Primer stream del tipo pedido (en video se ignoran las carátulas, igual que MediaProbe).
    const AVMediaType buscado = (tipo == Tipo::Video) ? AVMEDIA_TYPE_VIDEO : AVMEDIA_TYPE_AUDIO;
    for (unsigned i = 0; i < fmt_->nb_streams && !stream_; ++i) {
        AVStream* s = fmt_->streams[i];
        if (s->codecpar->codec_type == buscado && !(s->disposition & AV_DISPOSITION_ATTACHED_PIC)) stream_ = s;
    }
    if (!stream_) return fallar(tipo == Tipo::Video ? "no tiene video." : "no tiene audio.");

    // Los demás streams no interesan: el demuxer los descarta sin leerlos.
    for (unsigned i = 0; i < fmt_->nb_streams; ++i)
        if (fmt_->streams[i] != stream_) fmt_->streams[i]->discard = AVDISCARD_ALL;

    const AVCodec* dec = avcodec_find_decoder(stream_->codecpar->codec_id);
    if (!dec) return fallar(std::string("códec no soportado (") + avcodec_get_name(stream_->codecpar->codec_id) + ").");
    codec_ = avcodec_alloc_context3(dec);
    if (!codec_) return fallar("sin memoria.");
    avcodec_parameters_to_context(codec_, stream_->codecpar);
    codec_->pkt_timebase = stream_->time_base;
    codec_->thread_count = 0;  // 0 = FFmpeg elige la cantidad de hilos
    err = avcodec_open2(codec_, dec, nullptr);
    if (err < 0) return fallar(mensajeErrorFFmpeg(err));

    frameRate_ = av_guess_frame_rate(fmt_, stream_, nullptr);
    paquete_ = av_packet_alloc();
    salida_ = fmt_->duration > 0 ? fmt_->duration : INT64_MAX;
    return true;
}

bool MediaDecoder::setRange(model::TimeUs entrada, model::TimeUs salida, std::string* error) {
    entrada_ = entrada;
    salida_ = salida;
    primerCuadro_ = true;
    finArchivo_ = false;
    terminado_ = false;

    if (entrada > 0) {
        // BACKWARD: el keyframe más cercano ANTES de la entrada (si no, faltarían cuadros).
        const int err = av_seek_frame(fmt_, stream_->index, aPts(entrada), AVSEEK_FLAG_BACKWARD);
        if (err < 0) {
            if (error) *error = "Error al buscar la posición: " + mensajeErrorFFmpeg(err);
            return false;
        }
    } else {
        av_seek_frame(fmt_, stream_->index, stream_->start_time != AV_NOPTS_VALUE ? stream_->start_time : 0,
                      AVSEEK_FLAG_BACKWARD);
    }
    avcodec_flush_buffers(codec_);  // descartar lo que el decodificador tenía antes del seek
    return true;
}

int MediaDecoder::decodificarSiguiente(AVFrame* out) {
    for (;;) {
        // ¿El decodificador ya tiene un cuadro listo?
        int err = avcodec_receive_frame(codec_, out);
        if (err == 0) return 1;
        if (err == AVERROR_EOF) return 0;
        if (err != AVERROR(EAGAIN)) return err;

        // Necesita más datos: leer el siguiente paquete de NUESTRO stream.
        if (finArchivo_) return 0;
        err = av_read_frame(fmt_, paquete_);
        if (err == AVERROR_EOF) {
            finArchivo_ = true;
            avcodec_send_packet(codec_, nullptr);  // paquete nulo = "drenar" los cuadros pendientes
            continue;
        }
        if (err < 0) return err;
        if (paquete_->stream_index == stream_->index) err = avcodec_send_packet(codec_, paquete_);
        av_packet_unref(paquete_);
        if (err < 0 && err != AVERROR(EAGAIN) && err != AVERROR_INVALIDDATA) return err;
    }
}

int MediaDecoder::nextFrame(AVFrame* out) {
    if (terminado_) return 0;
    for (;;) {
        av_frame_unref(out);
        const int r = decodificarSiguiente(out);
        if (r <= 0) {
            terminado_ = true;
            return r;
        }
        // best_effort_timestamp es el pts más fiable que conoce el decodificador.
        out->pts = out->best_effort_timestamp;
        if (out->pts == AV_NOPTS_VALUE) continue;

        const model::TimeUs inicio = aMicros(out->pts);
        model::TimeUs dur = 0;
        if (tipo_ == Tipo::Audio && out->sample_rate > 0)
            dur = static_cast<model::TimeUs>(out->nb_samples) * model::kMicrosPorSegundo / out->sample_rate;
        else if (out->duration > 0)
            dur = aMicros(out->duration);
        else if (frameRate_.num > 0)
            dur = av_rescale_q(1, av_inv_q(frameRate_), kMicros);

        if (inicio >= salida_) {  // pasamos la salida del rango
            terminado_ = true;
            av_frame_unref(out);
            return 0;
        }
        if (inicio + dur <= entrada_) continue;  // termina antes de la entrada: se descarta

        // Video: el primer cuadro (que puede empezar un poco antes de la entrada porque
        // cubre ese instante) se "mueve" a la entrada exacta.
        if (tipo_ == Tipo::Video && primerCuadro_ && inicio < entrada_) out->pts = aPts(entrada_);
        primerCuadro_ = false;
        return 1;
    }
}

} // namespace engine
