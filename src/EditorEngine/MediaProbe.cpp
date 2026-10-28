// MediaProbe.cpp
// Capa: ENGINE. Ver MediaProbe.h.
//
// Pasos con FFmpeg:
//   1. avformat_open_input        -> abre el contenedor (mp4, mkv, mp3...) en solo lectura.
//   2. avformat_find_stream_info  -> lee un poco del archivo para conocer sus streams.
//   3. Se elige el primer stream de video (que no sea una carátula) y el primero de audio.
//   4. avcodec_find_decoder       -> comprueba que hay decodificador para cada stream.
#include "EditorEngine/MediaProbe.h"
#include "EditorEngine/FFmpegError.h"
#include "EditorEngine/ffmpeg_includes.h"

#include <filesystem>
#include <memory>

namespace engine {

namespace {

// RAII: cierra el AVFormatContext automáticamente al salir del ámbito.
struct CerrarFormato {
    void operator()(AVFormatContext* ctx) const { avformat_close_input(&ctx); }
};
using FormatoPtr = std::unique_ptr<AVFormatContext, CerrarFormato>;

std::filesystem::path pathDesdeUtf8(const std::string& utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

ProbeResult fallo(const std::string& ruta, const std::string& causa) {
    ProbeResult r;
    r.ok = false;
    r.error = "No se puede importar \"" + ruta + "\": " + causa;
    return r;
}

std::string nombreCodec(AVCodecID id) {
    const char* nombre = avcodec_get_name(id);
    return nombre ? nombre : "desconocido";
}

} // namespace

ProbeResult MediaProbe::probe(const std::string& rutaUtf8) {
    // Comprobación previa con std::filesystem: da un mensaje más claro que FFmpeg.
    std::error_code ec;
    const auto ruta = pathDesdeUtf8(rutaUtf8);
    if (!std::filesystem::exists(ruta, ec))
        return fallo(rutaUtf8, "el archivo no existe.");
    if (!std::filesystem::is_regular_file(ruta, ec))
        return fallo(rutaUtf8, "no es un archivo.");

    // 1. Abrir el contenedor.
    AVFormatContext* crudo = nullptr;
    int err = avformat_open_input(&crudo, rutaUtf8.c_str(), nullptr, nullptr);
    if (err < 0)
        return fallo(rutaUtf8, "no es un archivo multimedia válido (" + mensajeErrorFFmpeg(err) + ").");
    FormatoPtr fmt(crudo);

    // 2. Leer la información de los streams.
    err = avformat_find_stream_info(fmt.get(), nullptr);
    if (err < 0)
        return fallo(rutaUtf8, "no se pudo leer la información de sus streams (" + mensajeErrorFFmpeg(err) + ").");

    // 3. Elegir streams. Las carátulas de los mp3 aparecen como un "video" de una sola
    //    imagen (AV_DISPOSITION_ATTACHED_PIC): se ignoran para que el mp3 sea solo audio.
    const AVStream* video = nullptr;
    const AVStream* audio = nullptr;
    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        const AVStream* s = fmt->streams[i];
        const AVMediaType tipo = s->codecpar->codec_type;
        if (tipo == AVMEDIA_TYPE_VIDEO && !video && !(s->disposition & AV_DISPOSITION_ATTACHED_PIC))
            video = s;
        else if (tipo == AVMEDIA_TYPE_AUDIO && !audio)
            audio = s;
    }
    if (!video && !audio)
        return fallo(rutaUtf8, "no contiene streams de video ni de audio.");

    // 4. Comprobar que hay decodificador para cada stream elegido.
    if (video && !avcodec_find_decoder(video->codecpar->codec_id))
        return fallo(rutaUtf8, "códec de video no soportado (" + nombreCodec(video->codecpar->codec_id) + ").");
    if (audio && !avcodec_find_decoder(audio->codecpar->codec_id))
        return fallo(rutaUtf8, "códec de audio no soportado (" + nombreCodec(audio->codecpar->codec_id) + ").");

    ProbeResult r;
    model::MediaInfo& info = r.info;
    info.path = rutaUtf8;

    if (video) {
        info.hasVideo = true;
        info.width = video->codecpar->width;
        info.height = video->codecpar->height;
        info.videoCodec = nombreCodec(video->codecpar->codec_id);
        // av_guess_frame_rate combina la información del contenedor y del stream.
        const AVRational fr = av_guess_frame_rate(fmt.get(), const_cast<AVStream*>(video), nullptr);
        info.fps = model::Rational{fr.num, fr.den};
        if (info.width <= 0 || info.height <= 0)
            return fallo(rutaUtf8, "el stream de video no tiene una resolución válida.");
    }
    if (audio) {
        info.hasAudio = true;
        info.sampleRate = audio->codecpar->sample_rate;
        info.channels = audio->codecpar->ch_layout.nb_channels;
        info.audioCodec = nombreCodec(audio->codecpar->codec_id);
    }

    // Duración: el contenedor la da en AV_TIME_BASE (= microsegundos, igual que TimeUs).
    // Si no la tiene, se usa la del stream más largo, reescalada a microsegundos.
    model::TimeUs duracion = (fmt->duration != AV_NOPTS_VALUE) ? fmt->duration : 0;
    if (duracion <= 0) {
        for (const AVStream* s : {video, audio}) {
            if (s && s->duration != AV_NOPTS_VALUE) {
                const auto d = av_rescale_q(s->duration, s->time_base, AVRational{1, AV_TIME_BASE});
                if (d > duracion) duracion = d;
            }
        }
    }
    if (duracion <= 0)
        return fallo(rutaUtf8, "no se pudo determinar su duración.");
    info.duration = duracion;

    r.ok = true;
    return r;
}

} // namespace engine
