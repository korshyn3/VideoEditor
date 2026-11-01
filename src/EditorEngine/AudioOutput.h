// AudioOutput.h
// Capa: ENGINE (interno).
//
// Salida de audio del preview con XAudio2 (incluido en Windows 10/11): una "source voice"
// de 48 kHz, estéreo, float. Se le envían bloques de audio que reproduce en orden.
//
// También es el RELOJ MAESTRO del preview: samplesPlayed() dice cuántas muestras sonaron
// realmente, así el video se sincroniza con lo que se oye (design.md, D8).
// Si no hay dispositivo de audio, disponible() es false y el preview usa un reloj de pared.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

struct IXAudio2;
struct IXAudio2MasteringVoice;
struct IXAudio2SourceVoice;

namespace engine {

class AudioOutput {
public:
    static constexpr int kSampleRate = 48000;
    static constexpr int kCanales = 2;
    static constexpr int kMaxEnCola = 6;  // bloques en cola (~120 ms con bloques de 20 ms)

    AudioOutput() = default;
    ~AudioOutput();
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    // Debe llamarse desde el hilo que usará la salida (requiere COM inicializado).
    bool init();
    bool disponible() const { return voz_ != nullptr; }

    // Descarta todo lo encolado y reinicia el contador de muestras (para seek/rebuild).
    void reiniciar();
    void reproducir();
    void pausar();

    int enCola() const;
    bool aceptaMas() const { return enCola() < kMaxEnCola; }
    // Copia y encola `muestras` muestras estéreo intercaladas (L, R, L, R...).
    void enviar(const float* intercalado, int muestras);
    // Muestras reproducidas desde el último reiniciar().
    std::uint64_t samplesPlayed() const;

private:
    bool crearVoz();

    IXAudio2* xaudio_ = nullptr;
    IXAudio2MasteringVoice* master_ = nullptr;
    IXAudio2SourceVoice* voz_ = nullptr;
    // Anillo de búferes: XAudio2 lee la memoria mientras reproduce, así que cada bloque
    // vive en su propio búfer hasta que se reutiliza (hay más búferes que kMaxEnCola).
    std::array<std::vector<float>, kMaxEnCola + 2> anillo_;
    int siguiente_ = 0;
};

} // namespace engine
