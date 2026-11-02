// EncoderCatalog.cpp
// Capa: ENGINE. Ver EncoderCatalog.h.
#include "EditorEngine/EncoderCatalog.h"
#include "EditorEngine/ffmpeg_includes.h"

#include <mutex>

namespace engine {

namespace {

struct Candidato {
    const char* nombre;
    const char* descripcion;
    bool video;
};

// Los candidatos de la build de FFmpeg del proyecto (design.md: no hay libx264 ni libvpx).
// Si FFmpeg se recompila con otros encoders, basta con agregarlos aquí.
const Candidato kCandidatos[] = {
    {"h264_mf", "H.264 (MediaFoundation)", true},
    {"hevc_mf", "HEVC / H.265 (MediaFoundation)", true},
    {"mpeg4", "MPEG-4 Part 2 (software)", true},
    {"aac", "AAC", false},
    {"aac_mf", "AAC (MediaFoundation)", false},
};

// Primer formato que admite el encoder (de la lista de su configuración), o `porDefecto`.
template <class T>
T primerFormato(const AVCodec* codec, AVCodecConfig config, T porDefecto) {
    const void* valores = nullptr;
    int n = 0;
    if (avcodec_get_supported_config(nullptr, codec, config, 0, &valores, &n) >= 0 && valores && n > 0)
        return static_cast<const T*>(valores)[0];
    return porDefecto;
}

// Abre el encoder con parámetros mínimos para saber si funciona en este equipo.
bool seAbre(const AVCodec* codec, bool video) {
    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    if (!ctx) return false;
    if (video) {
        ctx->width = 64;
        ctx->height = 64;
        ctx->time_base = {1, 25};
        ctx->framerate = {25, 1};
        ctx->pix_fmt = primerFormato(codec, AV_CODEC_CONFIG_PIX_FORMAT, AV_PIX_FMT_YUV420P);
        ctx->bit_rate = 500000;
    } else {
        ctx->sample_rate = 48000;
        av_channel_layout_default(&ctx->ch_layout, 2);
        ctx->sample_fmt = primerFormato(codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, AV_SAMPLE_FMT_FLTP);
        ctx->time_base = {1, 48000};
        ctx->bit_rate = 128000;
    }
    const bool ok = avcodec_open2(ctx, codec, nullptr) >= 0;
    avcodec_free_context(&ctx);
    return ok;
}

} // namespace

const std::vector<EncoderInfo>& EncoderCatalog::disponibles() {
    static std::vector<EncoderInfo> lista;
    static std::once_flag una;
    std::call_once(una, [] {
        for (const Candidato& c : kCandidatos) {
            const AVCodec* codec = avcodec_find_encoder_by_name(c.nombre);
            if (codec && seAbre(codec, c.video)) lista.push_back({c.nombre, c.descripcion, c.video});
        }
    });
    return lista;
}

const char* EncoderCatalog::formato(model::Container contenedor) {
    switch (contenedor) {
    case model::Container::Mkv: return "matroska";
    case model::Container::Mov: return "mov";
    default: return "mp4";
    }
}

const char* EncoderCatalog::extension(model::Container contenedor) {
    switch (contenedor) {
    case model::Container::Mkv: return ".mkv";
    case model::Container::Mov: return ".mov";
    default: return ".mp4";
    }
}

bool EncoderCatalog::admite(model::Container contenedor, const std::string& encoder) {
    bool disponible = false;
    for (const EncoderInfo& e : disponibles()) disponible = disponible || e.nombre == encoder;
    if (!disponible) return false;
    const AVOutputFormat* fmt = av_guess_format(formato(contenedor), nullptr, nullptr);
    const AVCodec* codec = avcodec_find_encoder_by_name(encoder.c_str());
    return fmt && codec && avformat_query_codec(fmt, codec->id, FF_COMPLIANCE_NORMAL) == 1;
}

std::vector<EncoderInfo> EncoderCatalog::para(model::Container contenedor, bool video) {
    std::vector<EncoderInfo> r;
    for (const EncoderInfo& e : disponibles())
        if (e.video == video && admite(contenedor, e.nombre)) r.push_back(e);
    return r;
}

} // namespace engine
