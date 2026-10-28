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
    // Proyecto nuevo: una pista de video (V1), una de audio (A1) y ningún clip.
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
    OpResult addTrack(TrackKind kind, Id* nuevaPista = nullptr);
    // Elimina la pista y sus clips. Siempre debe quedar al menos una pista de video.
    OpResult removeTrack(Id trackId);

    // ---- Clips -------------------------------------------------------------
    // Coloca el medio completo al final de la pista. En una pista de video el medio
    // debe tener video (su audio queda vinculado); en una de audio debe tener audio.
    // Si es el primer clip del proyecto y tiene video, define el lienzo.
    OpResult appendMedia(Id trackId, Id mediaId, Id* nuevoClip = nullptr);

    // Mueve un clip a otra posición y/o a otra pista del mismo tipo. Se rechaza si se
    // solaparía con otro clip de la pista destino.
    OpResult moveClip(Id clipId, Id destTrackId, TimeUs newStart);

    // ---- Lienzo y exportación ----------------------------------------------
    OpResult setCanvas(const CanvasSettings& canvas);
    void setExportSettings(const ExportSettings& settings);

    // El proyecto se guardó: ya no hay cambios pendientes.
    void markSaved() { dirty_ = false; }

private:
    Track* findTrackMut(Id trackId);
    // Nombre siguiente para una pista nueva ("V2", "A3", ...).
    std::string nextTrackName(TrackKind kind) const;
    // true si [start, end) se solapa con algún clip de la pista (salvo ignorar).
    static bool overlaps(const Track& track, TimeUs start, TimeUs end, Id ignorar);
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
