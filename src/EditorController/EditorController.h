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

#include "EditorEngine/EncoderCatalog.h"
#include "EditorEngine/ExportJob.h"
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
    // Transición seleccionada, identificada por su clip de salida (o ninguna). Seleccionar
    // una transición deja sin clips seleccionados, y al revés.
    model::Id transicion = model::kSinId;

    bool contiene(model::Id id) const { return std::find(clips.begin(), clips.end(), id) != clips.end(); }
    bool multiple() const { return clips.size() > 1; }
};

// Resultado de abrir un proyecto: si se abrió, los archivos que no se encontraron (sus
// clips quedan offline: se ven en el timeline y se renderizan en negro y en silencio).
struct CargaResult {
    model::OpResult r;
    std::vector<std::string> faltantes;  // rutas UTF-8
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

    // Efectos de un clip de video (ver Project::addEffect). `indice` es la posición en la
    // lista de efectos del clip.
    model::OpResult addEffect(model::Id clipId, int tipo);
    model::OpResult setEffect(model::Id clipId, size_t indice, const model::Effect& efecto);
    model::OpResult moveEffect(model::Id clipId, size_t indice, int delta);
    model::OpResult removeEffect(model::Id clipId, size_t indice);

    // Tamaño, posición y opacidad de un clip de video (ver Project::setPipTransform).
    model::OpResult setPipTransform(model::Id clipId, const model::PipTransform& transform);

    // Transiciones (ver Project::addTransition). Si `fromClipId` es un audio vinculado se
    // usa su video. Agregar deja seleccionada la transición nueva.
    // Duración por defecto de una transición nueva (limitada a lo que permiten los clips).
    static constexpr model::TimeUs kDuracionTransicion = model::kMicrosPorSegundo;
    model::OpResult addTransition(model::Id fromClipId,
                                  model::TransitionStyle estilo = model::TransitionStyle::Dissolve,
                                  model::TimeUs duracion = kDuracionTransicion);
    model::OpResult setTransition(model::Id fromClipId, model::TransitionStyle estilo, model::TimeUs duracion);
    // Quita la transición seleccionada.
    model::OpResult removeSelectedTransition();
    // Clip de video del que sale una transición desde `clipId` (él mismo, o su video si es
    // un audio vinculado).
    model::Id clipDeVideo(model::Id clipId) const;

    // ---- Selección y cursor -------------------------------------------------
    void seleccionarPista(model::Id trackId);
    // Selecciona solo ese clip (y su pista).
    void seleccionarClip(model::Id clipId);
    // Ctrl+clic: agrega el clip a la selección o lo quita.
    void alternarSeleccionClip(model::Id clipId);
    void limpiarSeleccionClip();
    // Selecciona la transición que sale del clip (y ningún clip).
    void seleccionarTransicion(model::Id fromClipId);
    // Mueve el cursor (y el preview, si está iniciado) a t, limitado a [0, duración].
    void setPlayhead(model::TimeUs t);

    // ---- Proyecto (archivo JSON) ---------------------------------------------
    // Ruta del archivo del proyecto actual (vacía si nunca se guardó).
    const std::string& rutaProyecto() const { return rutaProyecto_; }
    // Guarda el proyecto en la ruta dada (y la recuerda); deja de haber cambios pendientes.
    model::OpResult guardar(const std::string& rutaUtf8);
    // Abre un proyecto. Si el archivo no es válido, el proyecto actual no cambia. Los
    // medios se vuelven a analizar: los que faltan quedan offline.
    CargaResult abrir(const std::string& rutaUtf8);
    // Proyecto nuevo y vacío.
    void nuevoProyecto();

    // ---- Exportación ----------------------------------------------------------
    // Mientras se exporta, las acciones que modifican el proyecto se rechazan.
    // Resolución de salida según el preset (o la personalizada).
    static void resolucionDe(const model::ExportSettings& s, int& width, int& height);
    // La ruta con la extensión del contenedor (la reemplaza o la agrega).
    static std::string conExtension(const std::string& rutaUtf8, model::Container contenedor);
    // Valida la configuración: ruta no vacía y distinta de todos los archivos fuente,
    // resolución par entre 16 y 7680, bitrates en rango y encoders disponibles para el
    // contenedor. (Si el archivo ya existe lo pregunta la View, con salidaExiste.)
    model::OpResult validarExportacion(const model::ExportSettings& s) const;
    static bool salidaExiste(const std::string& rutaUtf8);
    // Encoders de video o de audio que funcionan en este equipo y admite el contenedor.
    static std::vector<engine::EncoderInfo> encodersPara(model::Container contenedor, bool video);
    // Guarda la configuración en el proyecto y empieza a exportar en segundo plano.
    // `aviso` se llama desde el hilo de exportación al terminar.
    model::OpResult startExport(const model::ExportSettings& s, std::function<void()> aviso = {});
    void cancelExport();
    bool exportando() const { return export_ && export_->enCurso(); }
    double progresoExport() const { return export_ ? export_->progreso() : 0.0; }
    engine::ExportResult resultadoExport() const { return export_ ? export_->resultado() : engine::ExportResult{}; }
    // Espera a que termine la exportación en curso (para los tests).
    void esperarExport();

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
    // Deja el estado de edición como en un proyecto recién abierto.
    void reiniciarEstado();
    std::string rutaProyecto_;
    // Rechazo de las acciones que modifican el proyecto durante la exportación.
    static model::OpResult enExportacion();
    std::unique_ptr<engine::ExportJob> export_;         // la exportación (en curso o la última)
    std::unique_ptr<engine::PlaybackEngine> preview_;  // nullptr hasta iniciarPreview()
    std::uint64_t revisionEnPreview_ = UINT64_MAX;      // revisión enviada al preview
    bool seguiaReloj_ = false;                          // el cursor seguía al reloj del preview
};

} // namespace controller
