# Design

## Context

El proyecto parte de cero: el repositorio solo tiene `openspec/`. La motivación está en proposal.md y los requisitos en `specs/`. Estas son las restricciones del entorno que se verificaron:

- **Toolchain**: Windows 11 x64, Visual Studio 18 (2026) con MSVC (toolset v145) y MFC instalado.
- **Proyecto de referencia**: `E:\PrograSistemas\CambiarBMP`, una app MFC basada en diálogo (`CDialogEx`) con `.slnx` + `.vcxproj`, `UseOfMfc=Dynamic`, Unicode y C++20. Su interfaz se diseña en el `.rc` con el Editor de recursos, los botones se conectan por `MESSAGE_MAP` y el lienzo se dibuja a mano en `OnPaint` con el mouse capturado (`OnLButtonDown`/`OnMouseMove`/`OnLButtonUp`). Este editor sigue el mismo estilo.
- **FFmpeg n9.0.2** (build propio, `--enable-shared`, sin GPL) en `E:\PrograSistemas\FFmpeg_9\msvc`:
  - Versiones de las bibliotecas: avcodec-63, avformat-63, avfilter-12, avutil-61, swscale-10 y swresample-7.
  - **Los `.lib` de importación están en `msvc\bin`**, junto a las DLLs. `msvc\lib` solo contiene `.def` y `pkgconfig`.
  - Encoders de video: `h264_mf` y `hevc_mf` (MediaFoundation, por hardware), `mpeg4` (software) y `*_d3d12va`. **No hay libx264, libvpx ni libaom.** Encoders de audio: `aac` nativo y `aac_mf`. `opus` y `vorbis` existen, pero son experimentales.
  - Filtros disponibles: `xfade`, `acrossfade`, `overlay`, `fade`, `afade`, `hue`, `lutyuv`, `colorlevels`, `curves`, `gblur`, `avgblur`, `colorchannelmixer`, `scale`, `pad`, `fps`, `trim`, `atrim`, `setpts`, `adelay`, `amix`, `apad`, `aresample` y `format`. **Faltan `eq` y `boxblur`**, que son GPL.
  - Muxers disponibles: mp4, matroska y mov.
- **Hardware**: Intel UHD + NVIDIA RTX 3070 Laptop. Ninguna de las dos GPUs codifica AV1, y por eso se descartó webm.
- **Decisiones del usuario**: View en MFC como CambiarBMP, solución `.slnx`/`.vcxproj` (en lugar de CMake), audio del preview con XAudio2, C++20, y un solo change dividido en 9 fases.

## Goals / Non-Goals

**Goals:**
- Que las fronteras MVC sean verificables por el compilador. Cada capa es un proyecto `.vcxproj` propio; solo los proyectos que importan `FFmpeg.props` ven los headers de FFmpeg, y solo la app usa MFC.
- Que la interfaz se pueda editar visualmente en el Editor de recursos de Visual Studio, con el Asistente para clases y "Agregar controlador de eventos" disponibles.
- Que **un solo motor de render** genere tanto el preview como la exportación. Así lo que se ve es lo que se exporta, como pide la spec timeline-preview.
- Que el código sea didáctico: el grafo de filtros se genera como texto (la misma sintaxis que `ffmpeg -filter_complex`), de modo que se puede imprimir, leer y probar con `ffmpeg.exe`.
- Que el modelo se pueda probar con tests unitarios sin GUI ni FFmpeg.

**Non-Goals:**
- Aceleración por GPU del decode y de los filtros. Solo se usa el hardware a través de los encoders MF.
- Edición en tiempo real a resolución completa en cualquier equipo. El preview puede ir a menor resolución y descartar cuadros.
- Deshacer/rehacer, aunque el diseño de comandos del Controller lo deja preparado.
- Soporte para Linux y macOS. MFC ata la View a Windows; el Model, el Controller y el engine siguen siendo C++ estándar.

## Decisions

### D1. Solución y proyectos

```
VideoEditor.slnx
props/FFmpeg.props                 # macro FFmpegRoot, include, lib, copia de DLLs
props/Common.props                 # C++20, /utf-8, /W4, /permissive-, x64, salida común
third_party/nlohmann/json.hpp      # JSON (solo headers)
third_party/doctest/doctest.h      # tests (solo headers)
src/EditorModel/       EditorModel.vcxproj       (Static Library; UseOfMfc=false; solo STL + nlohmann)
src/EditorEngine/      EditorEngine.vcxproj      (Static Library; Model + FFmpeg.props + XAudio2)
src/EditorController/  EditorController.vcxproj  (Static Library; Model + Engine)
src/VideoEditor/       VideoEditor.vcxproj       (App MFC: View + main; .rc con los diálogos)
tests/EditorTests/     EditorTests.vcxproj       (Console; doctest; Model + Engine + Controller)
tools/generar_medios_prueba.ps1
```

El grafo de dependencias es `VideoEditor (View) → EditorController → {EditorEngine, EditorModel}` y `EditorEngine → EditorModel`. Las referencias entre proyectos se declaran con *Referencias de proyecto* de VS, para que el orden de compilación salga solo. **Solo `VideoEditor` tiene `UseOfMfc=Dynamic`**: el Model, el Engine y el Controller no pueden incluir `afxwin.h` y no usan `CString`. **Solo los proyectos que importan `FFmpeg.props` ven los headers de FFmpeg**, así que el Model no puede incluirlos.

Se descartó un único `.vcxproj` con carpetas porque no impediría un `#include <libavcodec/...>` en el modelo, y se descartó CMake porque en modo carpeta VS no ofrece el Asistente para clases ni "Agregar controlador de eventos" para MFC.

### D2. Enlace con FFmpeg y copia de DLLs (`FFmpeg.props`)

`props/FFmpeg.props` es una hoja de propiedades que importan EditorEngine, VideoEditor y EditorTests:
- Define la macro de usuario `FFmpegRoot = E:\PrograSistemas\FFmpeg_9\msvc`, editable en el Administrador de propiedades.
- Añade `$(FFmpegRoot)\include` a *Directorios de inclusión adicionales*.
- Añade `$(FFmpegRoot)\bin` (donde están los `.lib` de importación) a *Directorios de bibliotecas adicionales*, y `avformat.lib;avcodec.lib;avfilter.lib;swscale.lib;swresample.lib;avutil.lib` a *Dependencias adicionales*.
- En los proyectos ejecutables, el evento posterior a la compilación ejecuta `xcopy /d /y "$(FFmpegRoot)\bin\*.dll" "$(OutDir)"`, así F5 encuentra las DLLs.
- Antes de compilar, un `<Target>` con `<Error Condition="!Exists('$(FFmpegRoot)\include\libavcodec\avcodec.h')">` falla con un mensaje claro si la ruta es incorrecta.

Los headers de FFmpeg se incluyen dentro de `extern "C"` en un único header del engine (`ffmpeg_includes.h`). Se descartó pkg-config porque los `.pc` apuntan a `msvc/lib`, donde no están los `.lib`.

### D3. Dependencias de terceros y configuración común

No se usa vcpkg: las dos bibliotecas que quedan (`nlohmann/json` y `doctest`) son de un solo header y se copian en `third_party/` con su licencia. Así basta con abrir la solución y compilar, sin pasos previos, como en CambiarBMP. `props/Common.props` fija la plataforma x64, `/std:c++20`, `/utf-8`, `/W4`, `/permissive-`, `$(SolutionDir)third_party` en los includes y una carpeta de salida común (`$(SolutionDir)bin\$(Configuration)\`), para que los tests y la app compartan las DLLs copiadas.

### D4. Modelo de datos (C++ puro)

- **Tiempo**: `using TimeUs = int64_t;` (microsegundos) en todo el modelo. No se usan `double`, para que split y trim no acumulen errores de redondeo. La conversión a `AVRational` ocurre solo en el engine.
- **Entidades**:
  - `Project` contiene `MediaPool`, `Timeline`, `CanvasSettings {w, h, fps}`, `ExportSettings`, `dirty` y `revision`.
  - `MediaPool` guarda los `MediaInfo` (ruta UTF-8, duración, hasVideo/hasAudio, w, h, fps, códecs, sampleRate, channels, offline).
  - `Timeline` contiene un `vector<Track>`.
  - `Track` tiene `{id, kind: Video|Audio, name, muted, clips, transitions}`.
  - `Clip` tiene `{id, mediaId, sourceIn, sourceOut, timelineStart, linkedAudio, effects, transform}`.
  - `Effect` es un `std::variant<Grayscale, BrightnessContrast, Blur, Fade>`.
  - `Transition` tiene `{fromClipId, toClipId, style, duration}`.
  - `PipTransform` tiene `{x, y, scale, opacity}`.
- **Invariantes**: los clips de una pista no se solapan, *salvo* que exista una transición entre ellos, y en ese caso se solapan exactamente `duration`. Siempre hay al menos una pista de video. `sourceIn < sourceOut <= media.duration`. Las operaciones (`split`, `trim`, `move`, `remove(ripple)`, `addTransition`, `separateAudio`...) son métodos que validan y devuelven `OpResult {bool ok; std::string error;}`. No se usa `std::expected` porque es de C++23.
- **Notificación de cambios**: cada mutación correcta incrementa `Project::revision`. El preview compara revisiones para saber que debe reconstruir. Se eligió en lugar de observers/señales porque es más simple de seguir: la View compara la revisión al recibir un mensaje o en su timer y redibuja si cambió.
- **Transiciones como solapamiento**: agregar una transición de duración D desplaza a la izquierda en D al clip `to` y a todos los siguientes de la pista. Quitarla los devuelve. Es lo que exigen `xfade` y `acrossfade`, cuya salida dura `la + lb - D`. Se descartó tomar material extra de fuera de in/out (handles) porque complica el modelo y obligaría a que existan esos márgenes en la fuente.
- **Mapeo de estilos a xfade**: `dissolve→fade`, `wipe→wipeleft|wiperight`, `slide→slideleft|slideright` y `circle→circleopen`.

### D5. Persistencia JSON

`model/ProjectSerializer` usa nlohmann::json. Esta dependencia es aceptable en el modelo porque no es GUI ni FFmpeg. El formato lleva `"formatVersion": 1` y las rutas de medios en UTF-8 absolutas. Al cargar se valida todo antes de reemplazar el proyecto actual: se parsea a un `Project` temporal y solo se intercambia si es válido. Re-probar los medios (para detectar los offline) es tarea del controller, porque el modelo no conoce FFmpeg.

### D6. Engine: el grafo del timeline como texto (`FilterGraphBuilder`)

Es la decisión central. Para un `TimelineSnapshot`, una resolución de salida y un instante inicial `T`, el builder genera **una cadena filter_complex** y la lista de entradas (una por clip: `buffer` de video y/o `abuffer` de audio). La cadena tiene esta forma:

```
# Pista V1 = concat de segmentos (clips y huecos), cada uno con duración exacta en cuadros:
[in0] setpts=PTS-STARTPTS, scale=W:H:force_original_aspect_ratio=decrease, pad=W:H:(ow-iw)/2:(oh-ih)/2,
      setsar=1, fps=F, format=yuv420p, <efectos>, tpad=stop_mode=clone:stop_duration=1, trim=end_frame=N0 [s0];
color=c=black:s=WxH:r=F, format=yuv420p, setsar=1, trim=end_frame=N1 [s1];          # hueco
[s0][s1][s2] concat=n=3:v=1:a=0, setpts=PTS+T0/TB [vt0];                             # pista completa
[vt0][vt1] overlay=0:0:eof_action=pass [vout];   # pistas superiores (PiP, fase 8) encima de V1

# Audio: cada pista también es un concat (audio vinculado de V1, pistas A1, A2...):
[in1] atrim=start=IN:end=OUT, asetpts=PTS-STARTPTS, aresample=48000,
      aformat=sample_fmts=fltp:channel_layouts=stereo, apad=whole_dur=D, atrim=duration=D [sa0];
anullsrc=r=48000:cl=stereo, aformat=..., atrim=duration=G [sa1];                    # silencio
[sa0][sa1] concat=n=2:v=0:a=1 [at0];
[at0][at1] amix=inputs=2:duration=longest:normalize=0, asetpts=PTS+T0/TB,
      aformat=sample_fmts=flt:sample_rates=48000:channel_layouts=stereo [aout]
```

- **Una pista = un `concat`** de segmentos que cubren desde `T0` hasta el final del timeline (los huecos son `color` negro o `anullsrc`). Se descartó superponer cada clip con su propio `overlay` sobre un fondo: `overlay` necesita el primer cuadro de *todas* sus entradas desde el arranque, así que todos los clips del timeline se decodificarían a la vez. `concat` pide datos solo al segmento actual, de modo que en cada pista hay **un único decoder activo** a la vez.
- Los límites de los segmentos de video se cuantizan a la rejilla de cuadros del proyecto (`round(t·F)`), así los segmentos encajan sin acumular error; `tpad`+`trim=end_frame` garantizan el número exacto de cuadros aunque la fuente termine antes. El audio usa tiempos exactos (precisión de muestra), con lo que la desincronía máxima es de medio cuadro y no se acumula.
- Los `pts` dentro de cada segmento empiezan en 0 (efectos como `fade` usan tiempos relativos al clip) y al final cada pista se lleva a **tiempo absoluto del timeline** con `setpts=PTS+T0/TB`.
- Las transiciones (fase 7) unen dos segmentos consecutivos con `xfade`/`acrossfade` antes del `concat`; las pistas superiores (fase 8) se componen sobre V1 con `overlay`.
- **Efectos**: escala de grises → `hue=s=0`; brillo/contraste → `lutyuv=y='clip((val-128)*C+128+B*255,0,255)'`, porque `eq` no está; desenfoque → `gblur=sigma=r`, porque `boxblur` no está; fade → `fade=t=in|out` (solo video, según la spec).
- **Arranque en T (seek)**: `T0` es T alineado hacia abajo a la rejilla de cuadros. Los clips que terminan antes de `T0` se omiten. Los que cruzan `T0` arrancan con `sourceIn' = sourceIn + (T0 - start)`. Si T cae dentro de una transición, `T0` retrocede al inicio de la transición y los cuadros con pts < T se descartan a la salida. El builder acepta además un instante final opcional: para mostrar un único cuadro en pausa solo se incluyen los clips de ese instante.
- **Ventaja didáctica y de depuración**: la cadena se registra en el log y los tests unitarios la comparan sin necesidad de FFmpeg.

Se consideró y descartó un *compositor por cuadro* hecho a mano (decodificar cada clip y mezclar píxeles en C++), porque no usa xfade ni acrossfade como pide la consigna y duplica lo que ya hace libavfilter. También se descartaron los filtros fuente `movie`/`amovie` dentro del grafo: ocultan la decodificación, que es parte del aprendizaje, y empeoran los mensajes de error.

### D7. Engine: decodificación y `GraphRunner`

- `MediaProbe` usa `avformat_open_input` + `avformat_find_stream_info` + `avcodec_find_decoder` para obtener el `MediaInfo`, y es lo que implementa el modo `--probe` de la fase 1.
- `MediaDecoder` (uno por entrada) abre el archivo, hace `av_seek_frame(BACKWARD)` y luego decodifica descartando hasta `sourceIn'`, para un seek preciso. Usa la API `send_packet`/`receive_frame` y `AVChannelLayout`, y ninguna API eliminada en FFmpeg 9.
- `GraphRunner` hace `avfilter_graph_parse_ptr` y `avfilter_graph_config` sobre la cadena del builder. Para alimentar el grafo, consulta `av_buffersrc_get_nb_failed_requests` para saber qué entradas piden datos, empuja cuadros de los decoders correspondientes y lee de `buffersink`/`abuffersink`. Fija `sample_fmt`, `ch_layout` y la frecuencia de muestreo del abuffersink para recibir trozos de audio regulares.
- Cada `MediaDecoder` se limita a su rango [in', out] y envía EOF a su buffersrc al terminar.

### D8. Preview: hilos y reloj

- **Hilo de la interfaz (MFC)**: lleva el bucle de mensajes. Solo este hilo toca ventanas, GDI y el modelo.
- **Hilo de render** (`PlaybackEngine`, `std::jthread`): recibe órdenes por una cola con política *último gana* (seek(T), play, pause, rebuild(snapshot)). Construye el grafo para la resolución de preview (ancho máximo de 960 px), convierte cada cuadro a **BGRA** con swscale (el formato que entiende `StretchDIBits`) y lo deja en una cola corta de 3 cuadros.
- **Audio con XAudio2**: un `IXAudio2SourceVoice` a 48 kHz, estéreo, float. El hilo de render le envía buffers de unos 20 ms (`SubmitSourceBuffer`) manteniendo 3 o 4 en cola. Se enlaza `xaudio2.lib` (XAudio2 2.9, incluido en Windows 10/11).
- **Reloj maestro = audio**: `GetState().SamplesPlayed` indica cuánto se oyó realmente. Un `SetTimer` de unos 15 ms en la ventana de preview elige el cuadro con el mayor pts ≤ reloj, descarta los atrasados e invalida la ventana. Como el grafo siempre produce audio (`anullsrc` de fondo), siempre hay reloj.
- **Snapshot inmutable**: cuando cambia `revision`, el controller copia el `Timeline` (por valor) y lo entrega al hilo de render. Así el engine nunca lee el modelo vivo y el modelo no necesita mutex. Las reconstrucciones se agrupan (debounce de unos 100 ms) mientras se arrastra un slider.
- **En pausa**: un seek o rebuild produce un solo cuadro. Ese trabajo sigue en el hilo de render, para no congelar la interfaz.
- **Avisos al hilo de la interfaz**: los hilos de trabajo nunca tocan ventanas; avisan con `PostMessage(hWnd, WM_APP_..., ...)` y la ventana consulta el estado en su handler.

### D9. Exportación

- `ExportJob` corre en un `std::jthread` propio. Usa el mismo `FilterGraphBuilder` con T=0 y la resolución de exportación (el lienzo se escala con `scale+pad`, conservando la relación de aspecto). Codifica con `avcodec_send_frame` y `receive_packet`, y escribe con `av_interleaved_write_frame`.
- **Disponibilidad de encoders**: al abrir el diálogo, `EncoderCatalog` prueba cada candidato con un `avcodec_open2` de prueba (64x64). Los candidatos son `h264_mf`, `hevc_mf` y `mpeg4` para video, y `aac` y `aac_mf` para audio. Solo se ofrecen los que abren y que el contenedor admite según `avformat_query_codec`. Esto es necesario porque los encoders MF pueden existir en la DLL y aun así fallar en un equipo concreto.
- **Formato de píxel**: se usa el primero de `codec->pix_fmts` (MF suele pedir `nv12`), y un `scale/format` final en el grafo lo convierte.
- **Progreso**: un `std::atomic<double>` con `ultimoPts / duración`. El diálogo de exportación lo lee con un `SetTimer` y lo muestra en un `CProgressCtrl`; al terminar, el job avisa con `PostMessage(WM_APP_EXPORT_DONE)`.
- **Cancelación**: con `std::stop_token`, que se comprueba entre cuadros. Al cancelar o fallar se cierra el contexto y se borra el archivo parcial con `std::filesystem::remove`.
- Mientras `ExportJob` está activo, el controller rechaza las operaciones que modifican el proyecto.

### D10. Controller y View (MFC)

- `EditorController` (C++ estándar, sin MFC) expone acciones de alto nivel (`importFiles`, `splitAtPlayhead`, `trimClip`, `deleteClip(ripple)`, `separateAudio`, `addEffect`, `setTransition`, `setPipTransform`, `save/load`, `startExport`/`cancelExport`...). Cada una valida, llama al modelo y devuelve `OpResult`. **La View nunca modifica el modelo:** lo lee como `const` y llama al controller. Los handlers `OnBnClicked...` deben tener pocas líneas: convertir los datos del control y llamar al controller.
- **Ventana principal**: `CEditorDlg : CDialogEx` (como `CCambiarBMPDlg`), diseñada en el `.rc`:
  - Un menú `IDR_MENU_PRINCIPAL` (Archivo, Editar, Pistas, Ayuda) y una tabla de aceleradores (Espacio = play/pausa, S = split, Supr / Shift+Supr = eliminar, Ctrl+S = guardar), que se aplica en `PreTranslateMessage`.
  - Una barra de botones (Importar, Play/Pausa, Split, Separar audio, Exportar).
  - Dos *Custom Control* (o `static` marcadores) para el preview y el timeline.
  - El panel de propiedades.
  - **Diseño dinámico** del Editor de recursos (propiedades *Moving/Sizing*) para que los controles se reacomoden al cambiar el tamaño de la ventana.
  - `OnOK`/`OnCancel` se sobrescriben para que Enter y Esc no cierren el editor; el cierre pasa por `OnClose`, que pregunta por los cambios sin guardar.
- **Controles dibujados a mano** (como el canvas de CambiarBMP), cada uno una clase `CWnd` propia:
  - `CPreviewWnd`: pinta el último cuadro BGRA con `StretchDIBits`, ajustado al tamaño con barras negras.
  - `CTimelineWnd`: dibuja la regla, las pistas, los clips, las transiciones (rombo) y el cursor con **doble buffer** (`CMemDC`, para que no parpadee). Maneja con el mouse (`SetCapture`) el clic en la regla, el arrastre del cursor, de clips y de bordes (trim), y un menú contextual. Tiene scroll horizontal y zoom con Ctrl+rueda. La conversión tiempo↔píxel vive en una clase sin MFC que se puede probar.
- **Panel de propiedades**: diálogos hijos (`WS_CHILD`, sin borde) diseñados cada uno en el Editor de recursos y mostrados según la selección: `IDD_PROP_MEDIO` (info), `IDD_PROP_CLIP` (in/out, efectos con `CListBox` + `CSliderCtrl`/`CEdit`, PiP con sliders) e `IDD_PROP_TRANSICION` (`CComboBox` de estilo y dirección, duración).
- **Diálogos modales**: `IDD_EXPORTAR` (contenedor, códecs, bitrates, presets o resolución personalizada, `CProgressCtrl`, Cancelar) y `CFileDialog` para importar (selección múltiple), abrir, guardar y elegir la salida.
- **Errores**: el controller devuelve el mensaje y la View lo muestra con `AfxMessageBox` (`MB_ICONERROR` o `MB_ICONWARNING`).

### D11. Rutas y codificación

El proyecto MFC es Unicode (`CString` es UTF-16, como en CambiarBMP). El Model, el Engine y el Controller guardan las rutas como `std::string` UTF-8 (lo que FFmpeg acepta en Windows) y usan `std::filesystem::path(std::u8string)` para el sistema de archivos. La conversión `CString ↔ UTF-8` ocurre solo en la View, en dos funciones de ayuda (`Utf8DesdeCString`/`CStringDesdeUtf8`, con `WideCharToMultiByte`/`MultiByteToWideChar` y `CP_UTF8`). Esto evita roturas con carpetas que llevan tildes o ñ.

### D12. Pruebas

- **Unitarias (doctest)**: operaciones del modelo, invariantes, serialización de ida y vuelta, y las cadenas del `FilterGraphBuilder` para escenarios fijos.
- **De integración**: los medios de prueba se generan con `ffmpeg.exe -f lavfi testsrc2/sine` mediante `tools/generar_medios_prueba.ps1`, que el proyecto EditorTests ejecuta como evento previo a la compilación (solo si faltan los archivos).
- **Interfaz**: se prueba a mano, siguiendo los escenarios de las specs. Se exporta un timeline pequeño y se verifica con `MediaProbe` la duración, la resolución y los streams.

## Risks / Trade-offs

- [Los encoders `h264_mf`/`hevc_mf` respetan el bitrate de forma aproximada y exigen `nv12`] → Se configuran `bit_rate`, `rc_max_rate` y `rate_control=cbr` cuando el encoder lo admite; se documenta la tolerancia y queda `mpeg4` por software como alternativa garantizada.
- [`overlay` con un PiP que empieza más tarde que el fondo podría retener cuadros del fondo hasta que llegue el primer cuadro del PiP] → Se usa `eof_action=pass` y, si hace falta, se rellena el inicio de la entrada superior con `tpad=start_duration` transparente. Se cubre con un test de integración.
- [Reconstruir el grafo en cada seek y arrastre es costoso con muchos clips] → Debounce, política último gana y preview a menor resolución. Solo se abren decoders para los clips que cruzan T.
- [`acrossfade` exige que ambos lados tengan audio] → El builder inserta `anullsrc` con la duración del clip sin audio, para que el crossfade sea simétrico.
- [`amix` con `normalize=0` puede saturar si se suman varias pistas fuertes] → Es aceptable para el alcance. Se documenta y queda como mejora la ganancia por pista.
- [Copiar el timeline por valor en cada revisión] → Los timelines académicos son pequeños, de decenas de clips, y el costo es despreciable frente a reconstruir el grafo.
- [El timeline de MFC tiene bastante código propio de dibujo y mouse, y puede parpadear] → Se aísla en `CTimelineWnd` con doble buffer (`CMemDC`) y `OnEraseBkgnd` que devuelve TRUE; la conversión tiempo↔píxel vive en una clase sin MFC con tests.
- [En un diálogo, teclas como Espacio, Supr o Enter las consume el control con foco] → Se usa una tabla de aceleradores en `PreTranslateMessage` y se sobrescriben `OnOK`/`OnCancel`.
- [XAudio2 requiere inicializar COM en el hilo que lo crea] → `CoInitializeEx(COINIT_MULTITHREADED)` al iniciar el hilo de render, y liberación ordenada al cerrar.
- [Cambiar de build de FFmpeg (otra versión de DLL)] → Se enlaza con los `.lib` sin número de versión, el post-build copia `*.dll` y la ruta es la macro `FFmpegRoot` de `FFmpeg.props`.

## Migration Plan

No aplica: es un proyecto nuevo, sin datos ni usuarios previos. El formato JSON nace con `formatVersion: 1` para permitir migraciones futuras.

## Open Questions

- Si más adelante se recompila FFmpeg con libx264 o libvpx, `EncoderCatalog` los detectará solos (solo hay que añadirlos a la lista de candidatos). Agregar webm de nuevo sería un change aparte.
