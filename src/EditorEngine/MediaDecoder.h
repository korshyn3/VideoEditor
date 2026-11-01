// MediaDecoder.h
// Capa: ENGINE (interno: usa tipos de FFmpeg; no lo incluye el Controller).
//
// Decodifica UN stream (el de video o el de audio) de un archivo, limitado a un rango
// [entrada, salida) del archivo fuente. Es la fuente de cuadros de cada entrada del
// grafo de filtros (GraphRunner).
//
// Flujo con FFmpeg:
//   av_read_frame -> avcodec_send_packet -> avcodec_receive_frame   (API send/receive)
// Seek preciso: av_seek_frame(BACKWARD) salta al keyframe ANTERIOR a la entrada; después
// se decodifica y se descartan los cuadros que terminan antes de la entrada.
#pragma once

#include "EditorEngine/ffmpeg_includes.h"
#include "EditorModel/Tiempo.h"

#include <string>

namespace engine {

class MediaDecoder {
public:
    enum class Tipo { Video, Audio };

    MediaDecoder() = default;
    ~MediaDecoder();
    MediaDecoder(const MediaDecoder&) = delete;
    MediaDecoder& operator=(const MediaDecoder&) = delete;

    // Abre el archivo (UTF-8) y prepara el decodificador del stream pedido.
    bool open(const std::string& rutaUtf8, Tipo tipo, std::string* error);

    // Limita la decodificación a [entrada, salida) (tiempos del archivo fuente) y hace seek.
    bool setRange(model::TimeUs entrada, model::TimeUs salida, std::string* error);

    // Devuelve el siguiente cuadro en `out` (pts en timeBase()).
    //   1 = cuadro válido, 0 = fin del rango (EOF), <0 = error de FFmpeg.
    // Video: el primer cuadro se ajusta para empezar exactamente en la entrada.
    int nextFrame(AVFrame* out);

    // Parámetros del stream (para configurar el buffersrc del grafo).
    AVRational timeBase() const { return stream_->time_base; }
    const AVCodecContext* codec() const { return codec_; }
    AVRational frameRate() const { return frameRate_; }
    Tipo tipo() const { return tipo_; }

private:
    int decodificarSiguiente(AVFrame* out);  // sin aplicar el rango
    model::TimeUs aMicros(int64_t pts) const;
    int64_t aPts(model::TimeUs t) const;

    AVFormatContext* fmt_ = nullptr;
    AVCodecContext* codec_ = nullptr;
    AVStream* stream_ = nullptr;
    AVPacket* paquete_ = nullptr;
    AVRational frameRate_{0, 1};
    Tipo tipo_ = Tipo::Video;

    model::TimeUs entrada_ = 0;
    model::TimeUs salida_ = 0;
    bool primerCuadro_ = true;    // aún no se entregó ningún cuadro del rango
    bool finArchivo_ = false;     // ya se envió el paquete nulo (drenado)
    bool terminado_ = false;      // se alcanzó la salida o el final
};

} // namespace engine
