// PlaybackEngine.cpp
// Capa: ENGINE. Ver PlaybackEngine.h y design.md (D8).
//
// Hilos:
//   - hilo de la interfaz: llama a setTimeline/seek/play/pause (solo anotan la orden) y a
//     tomarCuadro/reloj (leen resultados). Nunca espera al render.
//   - hilo de render (bucle()): ejecuta las órdenes, decodifica y llena las colas.
// Todo lo compartido está protegido por `m` o es atómico.
#include "EditorEngine/PlaybackEngine.h"
#include "EditorEngine/AudioOutput.h"
#include "EditorEngine/GraphRunner.h"
#include "EditorEngine/ffmpeg_includes.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>

namespace engine {

using model::TimeUs;
using Reloj = std::chrono::steady_clock;

struct PlaybackEngine::Impl {
    static constexpr size_t kCuadrosEnCola = 3;

    std::function<void()> aviso;

    // ---- Compartido entre hilos (protegido por m) ----
    std::mutex m;
    std::condition_variable_any cv;
    std::optional<TimelineSnapshot> snapPendiente;
    std::optional<TimeUs> seekPendiente;
    bool quiereReproducir = false;
    std::deque<FrameBGRA> cola;
    std::string error;
    std::atomic<TimeUs> reloj{0};
    std::atomic<bool> reproduciendo{false};

    // ---- Solo del hilo de render ----
    TimelineSnapshot snap;
    bool haySnap = false;
    TimeUs pos = 0;                         // dónde empieza (o empezará) la reproducción
    std::unique_ptr<GraphRunner> runner;
    GraphDescription grafo;
    AudioOutput audio;
    bool hayAudio = false;
    bool audioFin = false, videoFin = false, iniciado = false;
    Reloj::time_point inicioPared;          // reloj de pared si no hay dispositivo de audio
    SwsContext* sws = nullptr;
    AVFrame* frame = nullptr;

    std::jthread hilo;  // último miembro: arranca cuando todo lo demás ya existe

    explicit Impl(std::function<void()> a) : aviso(std::move(a)) {
        hilo = std::jthread([this](std::stop_token st) { bucle(st); });
    }
    ~Impl() {
        hilo.request_stop();
        cv.notify_all();
        if (hilo.joinable()) hilo.join();
    }

    void avisar() {
        if (aviso) aviso();
    }

    void fijarError(const std::string& e) {
        {
            std::lock_guard lk(m);
            error = e;
        }
        avisar();
    }

    // Convierte un cuadro yuv420p a BGRA con swscale.
    FrameBGRA convertir(const AVFrame* f, TimeUs pts) {
        FrameBGRA out;
        out.width = f->width;
        out.height = f->height;
        out.pts = pts;
        out.pixels.resize(static_cast<size_t>(f->width) * f->height * 4);
        sws = sws_getCachedContext(sws, f->width, f->height, static_cast<AVPixelFormat>(f->format), f->width, f->height,
                                   AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
        uint8_t* dst[4] = {out.pixels.data(), nullptr, nullptr, nullptr};
        int dstStride[4] = {f->width * 4, 0, 0, 0};
        sws_scale(sws, f->data, f->linesize, 0, f->height, dst, dstStride);
        return out;
    }

    // (Re)construye el grafo en `pos`. En pausa produce un único cuadro.
    void preparar(bool reproducir) {
        runner.reset();
        audio.reiniciar();
        audioFin = videoFin = iniciado = false;
        {
            std::lock_guard lk(m);
            cola.clear();
            error.clear();
        }
        if (!haySnap) return;

        const RenderSettings rs = FilterGraphBuilder::resolucionPreview(snap.canvas, kAnchoPreview);
        const TimeUs duracion = snap.timeline.duration();
        pos = std::clamp<TimeUs>(pos, 0, duracion);
        reloj = pos;

        if (reproducir) {
            grafo = FilterGraphBuilder::build(snap, rs, pos);
        } else {
            // Solo hace falta el cuadro de `pos`: se incluyen únicamente los clips de ese instante.
            grafo = FilterGraphBuilder::build(snap, rs, pos, pos + FilterGraphBuilder::tiempoDeCuadro(1, rs.fps));
        }
        if (grafo.vacio) {
            std::lock_guard lk(m);
            cola.push_back(FrameBGRA{});  // nada que mostrar: pantalla negra
            reproduciendo = false;
            quiereReproducir = false;
        } else {
            runner = std::make_unique<GraphRunner>();
            std::string err;
            if (!runner->build(grafo, &err)) {
                runner.reset();
                {
                    std::lock_guard lk(m);
                    reproduciendo = false;
                    quiereReproducir = false;
                }
                fijarError(err);
                return;
            }
            if (!reproducir) {
                TimeUs pts = 0;
                if (runner->pullVideo(frame, &pts) > 0) {
                    FrameBGRA c = convertir(frame, pts);
                    std::lock_guard lk(m);
                    cola.push_back(std::move(c));
                }
                av_frame_unref(frame);
                runner.reset();  // en pausa no se necesita más
            }
        }
        avisar();
    }

    // Un paso de la reproducción: llena las colas de audio y video y actualiza el reloj.
    void avanzar() {
        if (!runner) return;
        TimeUs pts = 0;

        // Audio primero: si se queda sin datos se oye un corte; un cuadro tarde no se nota tanto.
        while (hayAudio && !audioFin && audio.aceptaMas()) {
            const int r = runner->pullAudio(frame, &pts);
            if (r > 0) audio.enviar(reinterpret_cast<const float*>(frame->data[0]), frame->nb_samples);
            else audioFin = true;
            av_frame_unref(frame);
            if (r < 0) fijarError(runner->ultimoError());
        }
        if (!iniciado) {
            audio.reproducir();
            inicioPared = Reloj::now();
            iniciado = true;
        }

        for (;;) {
            {
                std::lock_guard lk(m);
                // Cuadros vencidos: si el siguiente también ya tocaba, el primero no se va a
                // mostrar nunca. Se descartan aquí por si la interfaz no los retira (p. ej.
                // ventana minimizada); si no, la cola se llenaría y el video se detendría.
                while (cola.size() > 1 && cola[1].pts <= reloj) cola.pop_front();
                if (videoFin || cola.size() >= kCuadrosEnCola) break;
            }
            const int r = runner->pullVideo(frame, &pts);
            if (r > 0) {
                FrameBGRA c = convertir(frame, pts);
                std::lock_guard lk(m);
                cola.push_back(std::move(c));
            } else {
                videoFin = true;
                if (r < 0) fijarError(runner->ultimoError());
            }
            av_frame_unref(frame);
        }

        // Reloj: muestras realmente reproducidas (o tiempo de pared si no hay audio).
        TimeUs t;
        if (hayAudio)
            t = grafo.t0 + static_cast<TimeUs>(audio.samplesPlayed() * model::kMicrosPorSegundo / AudioOutput::kSampleRate);
        else
            t = grafo.t0 + std::chrono::duration_cast<std::chrono::microseconds>(Reloj::now() - inicioPared).count();
        reloj = std::min(t, grafo.fin);

        // ¿Terminó? (todo el audio sonó y no queda video por decodificar)
        const bool audioTerminado = hayAudio ? (audioFin && audio.enCola() == 0) : (reloj >= grafo.fin);
        if (videoFin && audioTerminado) {
            pos = grafo.fin;
            reloj = grafo.fin;
            runner.reset();
            {
                std::lock_guard lk(m);
                reproduciendo = false;
                quiereReproducir = false;
            }
            avisar();
        }
    }

    void bucle(std::stop_token st) {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);  // XAudio2 usa COM
        hayAudio = audio.init();
        frame = av_frame_alloc();

        while (!st.stop_requested()) {
            bool reconstruir = false, hayOrden = false, quiere = false;
            {
                std::unique_lock lk(m);
                auto hayAlgo = [&] { return snapPendiente || seekPendiente || quiereReproducir != reproduciendo; };
                if (reproduciendo)
                    cv.wait_for(lk, st, std::chrono::milliseconds(5), hayAlgo);
                else
                    cv.wait(lk, st, hayAlgo);
                if (st.stop_requested()) break;

                if (snapPendiente) {
                    snap = std::move(*snapPendiente);
                    snapPendiente.reset();
                    haySnap = true;
                    reconstruir = true;
                    // Si se está reproduciendo, se sigue desde donde va el reloj.
                    if (reproduciendo) pos = reloj;
                }
                if (seekPendiente) {
                    pos = *seekPendiente;
                    seekPendiente.reset();
                    reconstruir = true;
                }
                quiere = quiereReproducir;
                hayOrden = quiere != reproduciendo;
                if (hayOrden) reproduciendo = quiere;
            }

            if (hayOrden && quiere) {
                // Play al final del timeline: vuelve a empezar.
                if (haySnap && pos >= snap.timeline.duration()) pos = 0;
                reconstruir = true;
            } else if (hayOrden && !quiere) {
                // Pausa: se queda en el reloj actual (el cuadro mostrado no cambia).
                pos = reloj;
                audio.pausar();
                runner.reset();
                avisar();
            }

            if (reconstruir) preparar(reproduciendo);
            if (reproduciendo) avanzar();
        }

        runner.reset();
        sws_freeContext(sws);
        av_frame_free(&frame);
        CoUninitialize();
    }
};

PlaybackEngine::PlaybackEngine(std::function<void()> aviso) : impl_(std::make_unique<Impl>(std::move(aviso))) {}

PlaybackEngine::~PlaybackEngine() = default;

void PlaybackEngine::setTimeline(TimelineSnapshot snap) {
    {
        std::lock_guard lk(impl_->m);
        impl_->snapPendiente = std::move(snap);
    }
    impl_->cv.notify_one();
}

void PlaybackEngine::seek(model::TimeUs t) {
    {
        std::lock_guard lk(impl_->m);
        impl_->seekPendiente = t;  // si había otro seek pendiente, este lo reemplaza
        impl_->reloj = std::max<model::TimeUs>(0, t);
    }
    impl_->cv.notify_one();
}

void PlaybackEngine::play() {
    {
        std::lock_guard lk(impl_->m);
        impl_->quiereReproducir = true;
    }
    impl_->cv.notify_one();
}

void PlaybackEngine::pause() {
    {
        std::lock_guard lk(impl_->m);
        impl_->quiereReproducir = false;
    }
    impl_->cv.notify_one();
}

bool PlaybackEngine::reproduciendo() const {
    return impl_->reproduciendo;
}

model::TimeUs PlaybackEngine::reloj() const {
    return impl_->reloj;
}

bool PlaybackEngine::tomarCuadro(FrameBGRA& out) {
    std::lock_guard lk(impl_->m);
    const model::TimeUs ahora = impl_->reloj;
    bool hay = false;
    // Se queda con el MÁS RECIENTE de los que ya tocan; los anteriores llegaron tarde.
    while (!impl_->cola.empty() && impl_->cola.front().pts <= ahora) {
        out = std::move(impl_->cola.front());
        impl_->cola.pop_front();
        hay = true;
    }
    return hay;
}

std::string PlaybackEngine::error() const {
    std::lock_guard lk(impl_->m);
    return impl_->error;
}

} // namespace engine
