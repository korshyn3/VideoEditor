# Proposal

## Why

Es un proyecto académico: un editor de video básico, no destructivo, escrito en C++20 sobre FFmpeg. Sirve para practicar una arquitectura MVC estricta y el uso directo de las bibliotecas de FFmpeg (libavformat, libavcodec, libavfilter, libswscale, libswresample, libavutil). Hoy el repositorio está vacío, salvo la carpeta de OpenSpec, así que este change define el producto completo desde cero.

## What Changes

- Nueva aplicación de escritorio para Windows hecha con MFC, en una solución de Visual Studio 2026 (`.slnx` + `.vcxproj`, MSVC v145), con la misma organización que el proyecto previo CambiarBMP. Se enlaza con el build propio de FFmpeg en `E:\PrograSistemas\FFmpeg_9\msvc`, y un evento post-build copia las DLLs de FFmpeg junto al ejecutable, para que funcione con F5.
- La arquitectura está separada en capas:
  - **Model**: Project, Timeline, Track, Clip, Effect, Transition y ExportSettings, en C++ puro y sin dependencias de GUI ni de FFmpeg.
  - **View**: MFC. Los diálogos (ventana principal, panel de propiedades, exportación y transiciones) se diseñan en el Editor de recursos de Visual Studio (`.rc`). El preview y el timeline multipista son controles propios que se dibujan a mano.
  - **Controller**: traduce las acciones del usuario en cambios del modelo.
  - **Engine/servicios**: es la única capa que llama a FFmpeg (probe, decodificación, grafos de filtros, mezcla de audio y encoding).
- Importa videos y audios (mp3, wav, aac) y los coloca en pistas.
- Permite editar el timeline con split en el cursor, trim de inicio y fin, eliminar clips y mover clips.
- Cada pista de video tiene su propia pista de audio, donde aparece automáticamente el audio de sus videos, vinculado al clip de video (se mueven, dividen, recortan y eliminan juntos). El audio se puede desvincular. Además hay pistas de audio generales para música y para audios desvinculados.
- Agrega efectos por clip mediante libavfilter: escala de grises, brillo/contraste, desenfoque y fade in/out.
- Agrega transiciones entre clips consecutivos de la misma pista con xfade (dissolve, wipe, slide y círculo) y una duración configurable. El audio hace crossfade (acrossfade). Las transiciones se pueden cambiar o quitar.
- Permite picture-in-picture: varias pistas de video, donde la superior se superpone con posición, escala y opacidad configurables.
- Preview en tiempo real con play/pausa y un cursor arrastrable, que muestra efectos, transiciones, overlays y audio mezclado.
- Exporta a mp4, mkv o mov (webm se descartó porque el build de FFmpeg no trae un encoder VP8/VP9/AV1 utilizable). Se elige el códec, el bitrate y la resolución (480p, 720p, 1080p o personalizada). La exportación corre en un hilo aparte, muestra el progreso y se puede cancelar.
- Guarda y carga proyectos en JSON.
- Los errores se informan con mensajes claros: archivo inválido o inexistente, códec no soportado, fallo al exportar.

## Capabilities

### New Capabilities

- `media-import`: abrir y analizar archivos de video y audio (duración, streams, resolución, fps, códecs) y rechazar con un mensaje claro los archivos inválidos o no soportados.
- `timeline-editing`: estructura multipista del timeline y operaciones sobre clips (agregar en orden, mover, split, trim y eliminar), siempre sin modificar los archivos originales.
- `audio-tracks`: pistas de audio de video (una por pista de video, con el audio vinculado de sus clips) y pistas de audio generales, importación de archivos de audio, desvinculación del audio y mezcla.
- `video-effects`: efectos por clip (escala de grises, brillo/contraste, desenfoque y fade in/out) con sus parámetros.
- `clip-transitions`: transiciones entre clips consecutivos de una misma pista, con estilo y duración, incluido el crossfade de audio.
- `video-compositing`: composición de varias pistas de video con overlay picture-in-picture (posición, escala y opacidad).
- `timeline-preview`: reproducción del timeline renderizado con play/pausa, seek mediante un cursor arrastrable y audio sincronizado.
- `video-export`: renderizado del timeline a un archivo con contenedor, códec, bitrate y resolución configurables, progreso, cancelación y ejecución en segundo plano.
- `project-persistence`: guardar y cargar el proyecto completo en JSON.

### Modified Capabilities

(ninguna: el proyecto no tiene specs existentes)

## Impact

- **Código nuevo**: la solución `VideoEditor.slnx` con un proyecto `.vcxproj` por capa (model, engine, controller, app MFC y tests), una hoja de propiedades `FFmpeg.props` y los recursos `.rc` de la interfaz.
- **Dependencias externas**:
  - FFmpeg propio (headers en `msvc\include`; los `.lib` de importación están en `msvc\bin`, junto a las DLLs: avcodec-63, avformat-63, avfilter-12, avutil-61, swscale-10, swresample-7).
  - MFC (componente de Visual Studio, el mismo que usa CambiarBMP) y XAudio2 (incluido en Windows) para el audio del preview.
  - Bibliotecas de solo headers incluidas en `third_party/`: `nlohmann/json` (JSON) y `doctest` (tests). No se usa vcpkg.
- **Plataforma**: solo Windows x64 con MSVC. La portabilidad a otros sistemas queda fuera del alcance.
- **Fuera del alcance**: deshacer/rehacer, keyframes y animación de parámetros, aceleración por GPU del decode/encode, títulos o texto, mezcla de audio avanzada (más allá de sumar pistas y el crossfade) y corrección de color avanzada.
