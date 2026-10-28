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
#include "EditorModel/Project.h"

#include <string>
#include <vector>

namespace controller {

struct Seleccion {
    model::Id trackId = model::kSinId;  // pista seleccionada (destino de importación)
    model::Id clipId = model::kSinId;   // clip seleccionado (o ninguno)
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
    // Importa archivos en el orden dado. Los que tienen video van a la pista de video
    // seleccionada (o a V1); los de solo audio, a la pista de audio seleccionada (o a la
    // primera de audio, que se crea si no existe).
    ImportResult importFiles(const std::vector<std::string>& rutasUtf8);

    model::OpResult addTrack(model::TrackKind kind);
    model::OpResult removeTrack(model::Id trackId);
    model::OpResult moveClip(model::Id clipId, model::Id destTrackId, model::TimeUs newStart);

    // ---- Selección y cursor -------------------------------------------------
    void seleccionarPista(model::Id trackId);
    // Selecciona el clip y también su pista.
    void seleccionarClip(model::Id clipId);
    void limpiarSeleccionClip();
    void setPlayhead(model::TimeUs t);

private:
    // Corrige la selección si la pista o el clip seleccionados ya no existen.
    void validarSeleccion();
    model::Id pistaDestino(model::TrackKind kind);

    model::Project project_;
    Seleccion seleccion_;
    model::TimeUs playhead_ = 0;
};

} // namespace controller
