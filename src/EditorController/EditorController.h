// EditorController.h
// Capa: CONTROLLER (C++ estándar; sin MFC). No incluye FFmpeg: usa el engine solo a
// través de sus headers públicos.
//
// Traduce las acciones del usuario (que llegan desde la View) en operaciones sobre el
// modelo y el engine. La View nunca modifica el modelo ni llama a FFmpeg directamente:
// lee el proyecto con project() (const) y pide cambios con los métodos de esta clase.
//
// Además guarda el ESTADO DE EDICIÓN que no forma parte del proyecto guardado: qué pista
// y qué clip están seleccionados y dónde está el cursor de reproducción.
#pragma once

#include "EditorEngine/MediaProbe.h"
#include "EditorEngine/PlaybackEngine.h"
#include "EditorModel/Project.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace controller {

struct Seleccion {
    model::Id trackId = model::kSinId;  // pista seleccionada (destino de importación)
    model::Id clipId = model::kSinId;   // clip principal: el último seleccionado (o ninguno)
    std::vector<model::Id> clips;       // todos los clips seleccionados (incluye el principal)

    bool contiene(model::Id id) const { return std::find(clips.begin(), clips.end(), id) != clips.end(); }
    bool multiple() const { return clips.size() > 1; }
};

// Resultado de importar varios archivos: los válidos se importan aunque otros fallen.
struct ImportResult {
    std::vector<model::Id> clips;     // clips creados, en orden
    std::vector<std::string> errores; // un mensaje por archivo rechazado
};

class EditorController {
public:
    EditorController();

    // ---- Lectura (para la View) ---------------------------------------------
    const model::Project& project() const { return project_; }
    const Seleccion& seleccion() const { return seleccion_; }
    model::TimeUs playhead() const { return playhead_; }

    // Analiza un archivo multimedia y devuelve su información o un error legible.
    engine::ProbeResult analizarArchivo(const std::string& rutaUtf8) const;

    // ---- Acciones del usuario -----------------------------------------------
    // Importa archivos en el orden dado, a continuación del último clip de la pista.
    //   - Videos: a la pista de video seleccionada (o la dueña de la pista de audio de
    //     video seleccionada, o V1), con su audio vinculado. Si la pista seleccionada es
    //     una pista de audio general, se coloca allí solo su audio, sin vínculo (y se
    //     rechazan los que no tienen audio).
    //   - Archivos de audio: a la pista general seleccionada o a la primera general
    //     (que se crea si no existe).
    ImportResult importFiles(const std::vector<std::string>& rutasUtf8);

    model::OpResult addTrack(model::TrackKind kind);
    model::OpResult removeTrack(model::Id trackId);
    // Mueve un clip (y su vinculado); con `sobrescribir` recorta los clips de debajo.
    // Después queda seleccionado solo ese clip.
    model::OpResult moveClip(model::Id clipId, model::Id destTrackId, model::TimeUs newStart,
                             bool sobrescribir = false);
    // Mueve todos los clips seleccionados `delta`, cada uno en su pista.
    model::OpResult moverSeleccion(model::TimeUs delta, bool sobrescribir = false);

    // Las acciones sobre la selección son "todo o nada": si alguna falla, el proyecto
    // vuelve a como estaba antes de la acción.
    // Divide en la posición del cursor todos los clips seleccionados que estén bajo el
    // cursor. La selección queda en las partes izquierdas.
    model::OpResult splitAtPlayhead();
    // Recorta un clip: nuevo tramo [newIn, newOut) de la fuente (ver Project::trimClip).
    // Solo con un clip seleccionado.
    model::OpResult trimClip(model::Id clipId, model::TimeUs newIn, model::TimeUs newOut);
    // Elimina todos los clips seleccionados, dejando huecos o cerrándolos (ripple).
    model::OpResult deleteSelectedClip(bool ripple);
    // Desvincula el clip seleccionado de su clip vinculado (video <-> audio). Solo con
    // un clip seleccionado.
    model::OpResult unlinkAudio();
    model::OpResult setTrackMuted(model::Id trackId, bool muted);

    // ---- Selección y cursor -------------------------------------------------
    void seleccionarPista(model::Id trackId);
    // Selecciona solo ese clip (y su pista).
    void seleccionarClip(model::Id clipId);
    // Ctrl+clic: agrega el clip a la selección o lo quita.
    void alternarSeleccionClip(model::Id clipId);
    void limpiarSeleccionClip();
    // Mueve el cursor (y el preview, si está iniciado) a t, limitado a [0, duración].
    void setPlayhead(model::TimeUs t);

    // ---- Preview -------------------------------------------------------------
    // Crea el motor de reproducción (hilo de render). `aviso` se llama desde ese hilo
    // cuando hay novedades; la View lo usa para hacer PostMessage a su ventana.
    void iniciarPreview(std::function<void()> aviso);
    // Detiene el hilo de render (llamar antes de destruir la ventana que recibe los avisos).
    void detenerPreview() { preview_.reset(); }
    // Envía al preview una copia del timeline si el modelo cambió desde el último envío.
    void sincronizarPreview();
    void reproducirPausar();
    bool reproduciendo() const;
    // Llamar periódicamente durante la reproducción: actualiza el cursor con el reloj del
    // preview y devuelve true si hay un cuadro nuevo para mostrar en `cuadro`.
    bool actualizarPreview(engine::FrameBGRA& cuadro);
    std::string errorPreview() const;

private:
    // Corrige la selección si la pista o el clip seleccionados ya no existen.
    void validarSeleccion();
    // Pista donde se coloca un archivo importado (ver importFiles).
    model::Id pistaDestino(bool tieneVideo);

    model::Project project_;
    Seleccion seleccion_;
    model::TimeUs playhead_ = 0;
    std::unique_ptr<engine::PlaybackEngine> preview_;  // nullptr hasta iniciarPreview()
    std::uint64_t revisionEnPreview_ = UINT64_MAX;      // revisión enviada al preview
    bool seguiaReloj_ = false;                          // el cursor seguía al reloj del preview
};

} // namespace controller
