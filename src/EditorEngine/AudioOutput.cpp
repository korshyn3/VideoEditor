// AudioOutput.cpp
// Capa: ENGINE. Ver AudioOutput.h.
#include "EditorEngine/AudioOutput.h"

#include <windows.h>
#include <xaudio2.h>

namespace engine {

AudioOutput::~AudioOutput() {
    if (voz_) voz_->DestroyVoice();
    if (master_) master_->DestroyVoice();
    if (xaudio_) xaudio_->Release();
}

bool AudioOutput::init() {
    if (FAILED(XAudio2Create(&xaudio_, 0, XAUDIO2_DEFAULT_PROCESSOR))) {
        xaudio_ = nullptr;
        return false;
    }
    if (FAILED(xaudio_->CreateMasteringVoice(&master_))) {  // falla si no hay dispositivo de audio
        master_ = nullptr;
        return false;
    }
    return crearVoz();
}

bool AudioOutput::crearVoz() {
    WAVEFORMATEX wf = {};
    wf.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wf.nChannels = kCanales;
    wf.nSamplesPerSec = kSampleRate;
    wf.wBitsPerSample = 32;
    wf.nBlockAlign = static_cast<WORD>(wf.nChannels * wf.wBitsPerSample / 8);
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    if (FAILED(xaudio_->CreateSourceVoice(&voz_, &wf))) {
        voz_ = nullptr;
        return false;
    }
    return true;
}

void AudioOutput::reiniciar() {
    if (!xaudio_ || !master_) return;
    // Destruir y recrear la voz es la forma más simple de descartar lo encolado y poner
    // el contador de muestras en cero (DestroyVoice espera a que XAudio2 la suelte).
    if (voz_) voz_->DestroyVoice();
    voz_ = nullptr;
    siguiente_ = 0;
    crearVoz();
}

void AudioOutput::reproducir() {
    if (voz_) voz_->Start();
}

void AudioOutput::pausar() {
    if (voz_) voz_->Stop();
}

int AudioOutput::enCola() const {
    if (!voz_) return 0;
    XAUDIO2_VOICE_STATE st = {};
    voz_->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    return static_cast<int>(st.BuffersQueued);
}

void AudioOutput::enviar(const float* intercalado, int muestras) {
    if (!voz_ || muestras <= 0) return;
    std::vector<float>& buf = anillo_[siguiente_];
    siguiente_ = (siguiente_ + 1) % static_cast<int>(anillo_.size());
    buf.assign(intercalado, intercalado + static_cast<size_t>(muestras) * kCanales);

    XAUDIO2_BUFFER xb = {};
    xb.AudioBytes = static_cast<UINT32>(buf.size() * sizeof(float));
    xb.pAudioData = reinterpret_cast<const BYTE*>(buf.data());
    voz_->SubmitSourceBuffer(&xb);
}

std::uint64_t AudioOutput::samplesPlayed() const {
    if (!voz_) return 0;
    XAUDIO2_VOICE_STATE st = {};
    voz_->GetState(&st);
    return st.SamplesPlayed;
}

} // namespace engine
