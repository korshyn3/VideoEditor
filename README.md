# Editor de video (C++20 · MFC · FFmpeg)

Editor de video básico y **no destructivo** para Windows, hecho como proyecto académico.
Usa una arquitectura **MVC estricta** y las bibliotecas de FFmpeg (libavformat, libavcodec,
libavfilter, libswscale, libswresample y libavutil). La planificación completa (specs,
diseño y tareas) está en `openspec/changes/video-editor-mvc-ffmpeg/`.

## Requisitos

- Visual Studio 2026 con la carga de trabajo **Desarrollo para el escritorio con C++** y el
  componente **MFC de C++** (toolset v145).
- El build de FFmpeg en `E:\PrograSistemas\FFmpeg_9\msvc` (headers en `include\`, `.lib` y
  DLLs en `bin\`). Para usar otra carpeta, cambia la macro `FFmpegRoot` en
  `props\FFmpeg.props` (o en el Administrador de propiedades de VS).

No hace falta instalar nada más: las bibliotecas de terceros (`nlohmann/json` y `doctest`) son
de un solo header y están en `third_party\`.

## Compilar y ejecutar

1. Abre `VideoEditor.slnx` en Visual Studio 2026.
2. Elige la configuración `Debug | x64` (o `Release | x64`).
3. Establece **VideoEditor** como proyecto de inicio y pulsa **F5**.

Después de compilar, las DLLs de FFmpeg se copian automáticamente a `bin\<Configuración>\`,
junto a `VideoEditor.exe` y `EditorTests.exe`.

### Tests

Ejecuta el proyecto **EditorTests** (o `bin\Debug\EditorTests.exe`). Antes de compilarlo,
`tools\generar_medios_prueba.ps1` genera con `ffmpeg.exe` los videos y audios de prueba que
faltan en `bin\<Configuración>\medios_prueba\`. `EditorTests.exe --help` muestra cómo filtrar
tests.

## Arquitectura: capas y dependencias

```
VideoEditor (View, MFC) ──► EditorController ──► EditorEngine ──► EditorModel
                                   │                  (FFmpeg)          ▲
                                   └────────────────────────────────────┘
```

| Proyecto | Capa | Puede usar | NO puede usar |
|---|---|---|---|
| `src/EditorModel` | **Model**: Project, Timeline, Track, Clip, Effect, Transition, ExportSettings, MediaInfo | STL, nlohmann/json | FFmpeg, MFC, Windows |
| `src/EditorEngine` | **Servicios/engine**: probe, decodificación, filtros, preview, exportación | Model, FFmpeg, XAudio2 | MFC |
| `src/EditorController` | **Controller**: traduce las acciones del usuario en cambios del modelo | Model, headers *públicos* del engine | FFmpeg, MFC |
| `src/VideoEditor` | **View** (MFC): diálogos del `.rc`, preview y timeline dibujados a mano | Controller y Model (solo lectura) | FFmpeg |
| `tests/EditorTests` | Tests (doctest) | Todo salvo MFC | — |

### Cómo se hacen cumplir las fronteras

El compilador hace cumplir las fronteras; no dependen solo de la disciplina:

- **FFmpeg**: solo los proyectos que importan `props\FFmpeg.props` (EditorEngine, VideoEditor
  y EditorTests) tienen los headers de FFmpeg en su ruta de includes. En EditorModel o
  EditorController, `#include <libavcodec/avcodec.h>` da
  `error C1083: Cannot open include file`. Dentro del engine, FFmpeg se incluye solo desde
  `ffmpeg_includes.h`, y solo desde archivos `.cpp`: los headers públicos del engine no
  exponen tipos de FFmpeg.
- **MFC**: solo VideoEditor tiene `UseOfMfc=Dynamic`. Si otra capa incluye `<afxwin.h>`, se
  produce `error C1189: Building MFC application with /MD[d] ... requires MFC shared dll version`.
- **Model ← View**: la View recibe el modelo como `const` y solo lo modifica a través del
  controller.

### Configuración compartida

- `props\Common.props`: C++20, `/utf-8`, `/W4`, `/permissive-`, `src\` y `third_party\` en los
  includes, y la salida común `bin\<Configuración>\` (todos los proyectos).
- `props\FFmpeg.props`: macro `FFmpegRoot`, headers y `.lib` de FFmpeg, error claro si la ruta
  es inválida y copia de las DLLs después de compilar.

### Texto y rutas

Los fuentes están en UTF-8. MFC trabaja con `CString` (UTF-16), y el resto de las capas con
`std::string` UTF-8, que es lo que acepta FFmpeg. La conversión ocurre solo en
`src\VideoEditor\Utf8.cpp`. Así funcionan las rutas con tildes o ñ.
