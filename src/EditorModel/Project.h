// Project.h
// Capa: MODEL (C++ estándar; sin FFmpeg ni MFC).
//
// Raíz del modelo: medios importados, timeline, lienzo y configuración de exportación.
//
// Todas las modificaciones pasan por métodos de esta clase, que:
//   1. VALIDAN la operación (y devuelven OpResult::fallo con un mensaje si no es válida,
//      sin cambiar nada);
//   2. aplican el cambio manteniendo los invariantes (clips ordenados y sin solapes,
//      al menos una pista de video...);
//   3. incrementan revision() y marcan el proyecto como modificado (dirty).
// Quien necesite enterarse de cambios (p. ej. el preview) compara revision().
#pragma once

#include "EditorModel/ExportSettings.h"
#include "EditorModel/MediaPool.h"
#include "EditorModel/OpResult.h"
#include "EditorModel/Timeline.h"

#include <cstdint>

namespace model {

// Resolución y fps de salida del proyecto (el "lienzo").
struct CanvasSettings {
    int width = 1920;
    int height = 1080;
    Rational fps{30, 1};
};

class Project {
public:
    // Proyecto nuevo: una pista de video (V1), su pista de audio (Audio V1), una pista de
    // audio general (A1) y ningún clip.
    Project();

    // ---- Lectura -----------------------------------------------------------
    const Timeline& timeline() const { return timeline_; }
    const MediaPool& media() const { return media_; }
    const CanvasSettings& canvas() const { return canvas_; }
    const ExportSettings& exportSettings() const { return export_; }
    std::uint64_t revision() const { return revision_; }
    bool dirty() const { return dirty_; }

    // ---- Medios ------------------------------------------------------------
    // Registra un medio ya analizado y devuelve su Id.
    Id addMedia(MediaInfo info);

    // ---- Pistas ------------------------------------------------------------
    // Una pista de video nueva va encima de las demás y trae su pista de audio de video;
    // una pista de audio nueva es siempre general.
    OpResult addTrack(TrackKind kind, Id* nuevaPista = nullptr);
    // Elimina la pista y sus clips (una pista de video, también su pista de audio).
    // Siempre debe quedar al menos una pista de video, y una pista de audio de video no
    // se puede eliminar sola. Los clips de otras pistas vinculados con los eliminados
    // quedan desvinculados.
    OpResult removeTrack(Id trackId);
    // Silencia (o vuelve a activar) una pista de audio.
    OpResult setTrackMuted(Id trackId, bool muted);

    // ---- Clips -------------------------------------------------------------
    // Coloca el medio completo al final de la pista.
    //   - Pista de video: el medio debe tener video. Si tiene audio, se crea además el
    //     clip de audio vinculado en la pista de audio de esa pista de video.
    //   - Pista de audio: el medio debe tener audio; se coloca solo el audio, sin vínculo.
    // Si es el primer clip del proyecto y tiene video, define el lienzo.
    OpResult appendMedia(Id trackId, Id mediaId, Id* nuevoClip = nullptr);

    // Mueve un clip a otra posición y/o a otra pista del mismo tipo. Si está vinculado,
    // su clip vinculado se mueve igual: el audio va a la pista de audio de la pista de
    // video destino (y un audio vinculado solo se mueve en el tiempo: su pista la decide
    // su video). Se rechaza entero si algún clip se solaparía con otro, salvo que se pida
    // sobrescribir: entonces los clips de debajo se recortan (ver moveClips).
    OpResult moveClip(Id clipId, Id destTrackId, TimeUs newStart, bool sobrescribir = false);

    // Mueve varios clips (y sus vinculados) el mismo tiempo, cada uno en su pista. Con
    // `sobrescribir`, los clips que quedarían debajo se recortan para dejar lugar: si
    // quedan tapados se eliminan, si se tapa su inicio o su final se recorta esa parte, y
    // si se tapa su medio se dividen y se quita lo tapado (con el mismo corte en su clip
    // vinculado). Sin sobrescribir, cualquier solape rechaza el movimiento entero.
    OpResult moveClips(const std::vector<Id>& clipIds, TimeUs delta, bool sobrescribir = false);

    // Divide el clip (y su vinculado) en el instante t del timeline: el original queda
    // como la parte izquierda y se crea un clip nuevo con la derecha (mismo medio,
    // efectos y transformación); las mitades derechas quedan vinculadas entre sí. Se
    // rechaza si t no está estrictamente dentro del clip.
    OpResult split(Id clipId, TimeUs t, Id* clipDerecho = nullptr);

    // Cambia el tramo de la fuente [newIn, newOut) del clip y de su vinculado. Mover el
    // punto de entrada mueve también el inicio en el timeline. Los valores se limitan a
    // la duración de la fuente; se rechaza si el clip dura menos de un cuadro del
    // proyecto o si alguno de los dos se solaparía con un vecino.
    OpResult trimClip(Id clipId, TimeUs newIn, TimeUs newOut);

    // Elimina el clip y su vinculado. Con ripple, en cada pista afectada los clips
    // posteriores se corren a la izquierda la duración del clip; sin ripple queda un hueco.
    OpResult removeClip(Id clipId, bool ripple);

    // Quita el vínculo entre un clip y su clip vinculado (se puede pedir desde cualquiera
    // de los dos). Los clips quedan donde están. Se rechaza si no hay vínculo.
    OpResult unlinkAudio(Id clipId);

    // Duración de un cuadro del lienzo (mínimo de un clip después de un trim).
    TimeUs frameDuration() const;

    // ---- Lienzo y exportación ----------------------------------------------
    OpResult setCanvas(const CanvasSettings& canvas);
    void setExportSettings(const ExportSettings& settings);

    // El proyecto se guardó: ya no hay cambios pendientes.
    void markSaved() { dirty_ = false; }

private:
    Track* findTrackMut(Id trackId);
    // Clip modificable y su pista (nullptr si no existe).
    Clip* findClipMut(Id clipId, Track** pista = nullptr);
    // Nombre siguiente para una pista nueva ("V2", "A3", ...).
    std::string nextTrackName(TrackKind kind) const;
    // true si [start, end) se solapa con algún clip de la pista (salvo ignorar).
    static bool overlaps(const Track& track, TimeUs start, TimeUs end, Id ignorar);
    // Inserta una pista de audio de video para la pista de video dada, después de las
    // demás pistas de audio de video (antes de las generales).
    void insertAudioTrackFor(const Track& pistaVideo);
    // Desvincula los clips cuyo clip vinculado ya no existe.
    void repararVinculos();
    // Destino de un clip en un movimiento.
    struct Destino {
        Id clipId;
        Id trackId;
        TimeUs start;
    };
    // Aplica los movimientos (agregando los audios vinculados) sobre una copia del
    // timeline, sobrescribiendo si se pide, y solo los acepta si no quedan solapes.
    OpResult aplicarMovimientos(std::vector<Destino> destinos, bool sobrescribir);
    // Quita del clip (y de su vinculado) el tramo [ini, fin) del timeline `tl`.
    void cortarRango(Timeline& tl, Id clipId, TimeUs ini, TimeUs fin);
    static void sortClips(Track& track);
    Id newId() { return nextId_++; }
    void touch();

    Timeline timeline_;
    MediaPool media_;
    CanvasSettings canvas_;
    ExportSettings export_;
    Id nextId_ = 1;
    std::uint64_t revision_ = 0;
    bool dirty_ = false;
};

} // namespace model
