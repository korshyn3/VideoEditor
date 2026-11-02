// ExportJob.h
// Capa: ENGINE (API pública, sin tipos de FFmpeg).
//
// Exporta el timeline completo a un archivo, en un hilo propio (std::jthread) para no
// bloquear la interfaz. Usa el mismo FilterGraphBuilder que el preview (con T = 0 y la
// resolución de exportación), codifica con avcodec_send_frame / avcodec_receive_packet y
// escribe con av_interleaved_write_frame. Ver design.md, D9.
//
//   - Progreso: atómico, último pts codificado / duración del timeline (0..1).
//   - Cancelación: std::stop_token, que se revisa entre cuadros.
//   - Si se cancela o falla, el archivo parcial se borra.
#pragma once

#include "EditorEngine/FilterGraphBuilder.h"
#include "EditorModel/ExportSettings.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

namespace engine {

struct ExportRequest {
    TimelineSnapshot snap;
    std::string rutaSalida;  // UTF-8, con la extensión del contenedor
    model::Container contenedor = model::Container::Mp4;
    std::string encoderVideo = "mpeg4";  // nombres de encoder de FFmpeg (ver EncoderCatalog)
    std::string encoderAudio = "aac";
    int videoKbps = 5000;
    int audioKbps = 192;
    int width = 1920;
    int height = 1080;
};

enum class ExportEstado { Inactivo, EnCurso, Terminado, Cancelado, Error };

struct ExportResult {
    ExportEstado estado = ExportEstado::Inactivo;
    std::string error;  // si estado == Error: mensaje con el códec o el paso que falló
};

class ExportJob {
public:
    // `alTerminar` se llama desde el hilo de exportación al terminar (bien, mal o
    // cancelada); la View lo usa para hacer PostMessage(WM_APP_EXPORT_DONE).
    explicit ExportJob(std::function<void()> alTerminar = {});
    ~ExportJob();  // cancela y espera al hilo
    ExportJob(const ExportJob&) = delete;
    ExportJob& operator=(const ExportJob&) = delete;

    // Empieza a exportar en segundo plano (false si ya hay una exportación en curso).
    bool start(ExportRequest pedido);
    void cancel();
    bool enCurso() const { return estado_.load() == ExportEstado::EnCurso; }
    double progreso() const { return progreso_.load(); }
    ExportResult resultado() const;

    // La exportación misma, síncrona (la usa el hilo; los tests la llaman directo).
    static ExportResult exportar(const ExportRequest& pedido, std::stop_token parar, std::atomic<double>* progreso);

private:
    std::function<void()> alTerminar_;
    std::atomic<ExportEstado> estado_{ExportEstado::Inactivo};
    std::atomic<double> progreso_{0.0};
    mutable std::mutex mutex_;
    std::string error_;
    std::jthread hilo_;
};

} // namespace engine
