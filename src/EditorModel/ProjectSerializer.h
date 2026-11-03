// ProjectSerializer.h
// Capa: MODEL (C++ estándar + nlohmann::json; sin FFmpeg ni MFC).
//
// Guarda el proyecto en un JSON legible y lo vuelve a cargar. El archivo lleva
// "formatVersion": 2, el lienzo, los medios (ruta UTF-8 absoluta y la información que
// dio el análisis, para poder abrir el proyecto aunque falte algún archivo), las pistas,
// los clips (con su vínculo, efectos y PiP), las transiciones y la última configuración
// de exportación. Nunca copia los medios.
//
// Cargar es "todo o nada": se arma un Project temporal y se valida entero (campos, rangos,
// referencias, vínculos y transiciones coherentes); solo si es válido se devuelve.
// Re-analizar los medios (para marcar los que faltan como offline) es tarea del
// controller, porque el modelo no conoce FFmpeg.
//
// Versión 1 (se sigue pudiendo abrir): la transformación no se aplicaba en V1, y sus clips
// guardaban un valor que nunca se usó. Al cargarla, los clips de V1 ocupan todo el lienzo.
#pragma once

#include "EditorModel/Project.h"

#include <string>

namespace model {

class ProjectSerializer {
public:
    static constexpr int kFormatVersion = 2;

    static std::string toJson(const Project& proyecto);
    // true si el texto es un proyecto válido (en `proyecto`); si no, el motivo en `error`.
    static bool fromJson(const std::string& texto, Project& proyecto, std::string* error);
};

} // namespace model
