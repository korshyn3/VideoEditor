# Tasks

## 1. Fase 1 — Solución, proyectos y enlace con FFmpeg

- [x] 1.1 Crear `VideoEditor.slnx` con los proyectos `EditorModel`, `EditorEngine`, `EditorController` (bibliotecas estáticas, `UseOfMfc=false`), `VideoEditor` (app MFC basada en diálogo, `UseOfMfc=Dynamic`, Unicode, v145) y `EditorTests` (consola), con referencias de proyecto según design D1, y un `README.md` que explique las capas; verificar que la solución abre en VS 2026 y compila vacía en Debug y Release x64
- [x] 1.2 Crear `props/Common.props` (x64, `/std:c++20 /utf-8 /W4 /permissive-`, `third_party` en includes, salida común `bin\$(Configuration)\`) y copiar `nlohmann/json.hpp` y `doctest.h` con sus licencias en `third_party/`; verificar que los cinco proyectos heredan las opciones en el Administrador de propiedades
- [x] 1.3 Crear `props/FFmpeg.props` (macro `FFmpegRoot`, include, `$(FFmpegRoot)\bin` como directorio de bibliotecas, los seis `.lib`, post-build `xcopy` de `*.dll` y `<Error>` si falta `avcodec.h`) e importarla en EditorEngine, VideoEditor y EditorTests; verificar que tras compilar las DLLs (avcodec-63, avformat-63, ...) están junto a `VideoEditor.exe`, y que cambiar `FFmpegRoot` a una carpeta inválida da un error claro
- [x] 1.4 Verificar las fronteras de capas: un `#include <libavcodec/avcodec.h>` o `#include <afxwin.h>` de prueba en EditorModel no compila; documentarlo en el README
- [x] 1.5 Implementar `ffmpeg_includes.h` (headers en `extern "C"`) y una función de ayuda que traduce códigos `AVERROR` a texto (`av_strerror`); verificar con un test doctest que `AVERROR(ENOENT)` produce un mensaje no vacío
- [x] 1.6 Crear `tools/generar_medios_prueba.ps1` (testsrc2 + sine en mp4, solo video, mp3, wav, aac y un .mp4 falso de texto, con `ffmpeg.exe` de `FFmpegRoot`) y engancharlo como evento previo a la compilación de EditorTests (solo si faltan los archivos); verificar que los archivos aparecen al compilar los tests
- [x] 1.7 Implementar `MediaInfo` (modelo) y `MediaProbe` (engine): duración, streams, resolución, fps, códecs, sample rate y canales, con errores para archivo inexistente, no multimedia y códec sin decoder; verificar con tests sobre los medios generados
- [x] 1.8 Crear el diálogo principal mínimo `CEditorDlg` con un botón "Abrir video…" (`CFileDialog`) y un cuadro de texto de solo lectura que muestra la información de `MediaProbe` o el error (conversión CString↔UTF-8 de design D11); verificar con F5 sobre un mp4 válido, sobre el .mp4 falso y sobre un archivo en una carpeta con tildes o ñ

## 2. Fase 2 — Modelo de timeline y GUI básica

- [x] 2.1 Implementar en EditorModel las entidades de design D4 (`TimeUs`, `Project`, `MediaPool`, `CanvasSettings`, `Timeline`, `Track`, `Clip`, `Effect` variant, `Transition`, `PipTransform`, `ExportSettings`, `OpResult`, `revision`/`dirty`); verificar que EditorModel compila sin FFmpeg ni MFC
- [x] 2.2 Implementar proyecto nuevo (V1 + A1), agregar/eliminar pistas (no permitir quitar la última de video) y lienzo tomado del primer video; verificar con tests de los escenarios de timeline-editing "Proyecto nuevo", "Agregar pistas", "Eliminar la única pista de video" y "Primer video define el lienzo"
- [x] 2.3 Implementar la colocación en orden de medios importados en la pista seleccionada y el movimiento de clips con rechazo de solapamiento; verificar con tests "Importar tres videos", "Mover a un hueco libre" y "Movimiento con solapamiento"
- [x] 2.4 Implementar `EditorController` con `importFiles` (probe por archivo, importa los válidos y devuelve los errores de los inválidos); verificar con un test de controller del escenario "Importación múltiple con un archivo inválido"
- [x] 2.5 Diseñar en el Editor de recursos la ventana principal `IDD_EDITOR` (redimensionable, con diseño dinámico), el menú `IDR_MENU_PRINCIPAL`, la tabla de aceleradores, la barra de botones y los marcadores del preview, el timeline y el panel de propiedades; sobrescribir `OnOK`/`OnCancel`/`OnClose`; verificar a mano que la ventana se redimensiona bien y que Enter/Esc no la cierran
- [x] 2.6 Implementar la clase sin MFC de conversión tiempo↔píxel (zoom y scroll) con tests, y `CTimelineWnd` (regla, pistas, clips con nombre, selección, cursor por clic, zoom con Ctrl+rueda, scroll horizontal, arrastre de clips con `SetCapture`, doble buffer con `CMemDC`); verificar a mano importando tres videos y moviéndolos entre V1 y V2 sin parpadeo
- [x] 2.7 Diseñar el diálogo hijo `IDD_PROP_MEDIO` en el panel de propiedades (info del medio o clip seleccionado), conectar "Importar…" (`CFileDialog` con selección múltiple) y mostrar errores con `AfxMessageBox`; verificar el escenario "Ver la información de un medio" y que importar el .mp4 falso muestra el error sin cambiar el timeline

## 3. Fase 3 — Decodificación y preview

- [x] 3.1 Implementar `MediaDecoder` (seek BACKWARD + descarte hasta el instante pedido, API send/receive, `AVChannelLayout`, EOF al pasar `out`); verificar con un test que el primer cuadro devuelto tras seek a 2.0 s tiene pts ≥ 2.0 s y < 2.0 s + 1 cuadro
- [x] 3.2 Implementar `FilterGraphBuilder` para la versión base (fondo `color`/`anullsrc`, clips de V1 con setpts absoluto + scale/pad/fps/format, audio vinculado con atrim/aresample/aformat/adelay, `amix normalize=0`, arranque en T); verificar con tests doctest que comparan la cadena generada para escenarios fijos y que se puede ejecutar a mano con `ffmpeg.exe -filter_complex`
- [x] 3.3 Implementar `GraphRunner` (parse + config, alimentar buffersrc según `av_buffersrc_get_nb_failed_requests`, leer buffersink/abuffersink con formato fijo 48 kHz estéreo float); verificar con un test de integración que renderiza 2 s de un timeline de dos clips y obtiene la cantidad esperada de cuadros y muestras
- [x] 3.4 Implementar `PlaybackEngine` en `std::jthread` con cola de órdenes "último gana" (play, pause, seek, rebuild(snapshot)), resolución de preview máx. 960 px, conversión a BGRA con swscale y cola de 3 cuadros; verificar que la ventana sigue respondiendo durante un seek a un archivo largo
- [x] 3.5 Implementar la salida de audio con XAudio2 (COM en el hilo de render, `IXAudio2SourceVoice` 48 kHz estéreo float, 3–4 buffers de ~20 ms en cola) y el reloj maestro con `SamplesPlayed`; verificar a mano los escenarios "Reproducir", "Fin del timeline" y "Timeline pesado" (sincronía con un video de prueba con pitido y destello)
- [x] 3.6 Implementar `CPreviewWnd` (`StretchDIBits` del último cuadro con barras negras, `SetTimer` de ~15 ms que elige el cuadro según el reloj) y conectar play/pausa (botón y Espacio) y el cursor arrastrable de `CTimelineWnd`; verificar los escenarios "Seek en pausa" y "Seek durante reproducción"
- [x] 3.7 Conectar `Project::revision` → snapshot por valor → rebuild con debounce de ~100 ms, y los avisos de los hilos de trabajo por `PostMessage(WM_APP_...)`; verificar que mover un clip en pausa actualiza el cuadro mostrado

## 4. Fase 4 — Cortes (split / trim / eliminar)

- [x] 4.1 Implementar `split` en el modelo y la acción "Dividir en cursor" (botón, menú Editar y tecla S); verificar con tests "Split en el medio", "Cursor fuera del clip" y "Split conserva el audio vinculado"
- [x] 4.2 Implementar `trim` (in/out con límites de fuente, mínimo 1 cuadro, sin solapar vecinos) por arrastre de bordes en `CTimelineWnd` (cursor de redimensionar) y por valores en `IDD_PROP_CLIP`; verificar con tests "Recortar el inicio" y "Extender más allá de la fuente"
- [x] 4.3 Implementar eliminar clip normal y con ripple (menú, menú contextual y teclas Supr / Shift+Supr); verificar con tests "Eliminar dejando hueco", "Eliminar con ripple" y "Eliminar un segmento intermedio", y en el preview que el hueco se ve negro y en silencio
- [x] 4.4 Agregar un test de no destructividad que guarda hash y fecha de modificación de los medios de prueba antes y después de editar; verificar que no cambian (escenario "Edición no destructiva")

## 5. Fase 5 — Pistas de audio (separar / agregar)

- [ ] 5.1 Implementar `separateAudio` en el modelo (clip de audio en pista libre o nueva, desvincular audio del video, rechazo si no hay audio); verificar con tests "Separar audio" y "Clip sin audio"
- [ ] 5.2 Extender `FilterGraphBuilder` con clips de pistas de audio (atrim + adelay) y omitir pistas silenciadas; verificar con tests de cadena y a mano los escenarios "Mover el audio separado", "Borrar el audio separado", "Música sobre video con audio" y "Pista silenciada"
- [ ] 5.3 Implementar importación de mp3/wav/aac en la pista de audio seleccionada y la colocación solo-audio de archivos de video en pistas de audio; verificar con tests "Importar música" e "Importar video en pista de audio"
- [ ] 5.4 Dibujar los clips de audio con otro color en `CTimelineWnd`, añadir el botón de mute en la cabecera de cada pista y la acción "Separar audio" (botón y menú contextual); verificar a mano el flujo completo separar → mover → borrar

## 6. Fase 6 — Efectos

- [ ] 6.1 Implementar en el modelo agregar/editar/reordenar/quitar efectos con rangos y límites (brillo −1..1, contraste 0..3, blur 0..50, fades que no superan la duración) y la copia de efectos en split (fade in solo en la 1.ª mitad, fade out solo en la 2.ª); verificar con tests "Valor fuera de rango", "Fade más largo que el clip" y "Split con efectos"
- [ ] 6.2 Traducir efectos a filtros en `FilterGraphBuilder` (`hue=s=0`, `lutyuv` para brillo/contraste, `gblur`, `fade` en tiempo absoluto); verificar con tests de cadena y con un test de integración que un cuadro con escala de grises tiene U=V≈128
- [ ] 6.3 Diseñar en `IDD_PROP_CLIP` la sección de efectos (`CListBox`, `CComboBox` para agregar, botones subir/bajar/quitar, `CSliderCtrl` + `CEdit` por parámetro) con actualización en vivo del preview; verificar a mano "Agregar un efecto", "Quitar un efecto", "Aumentar brillo", "Desenfocar" y "Fade in de 1 segundo"

## 7. Fase 7 — Transiciones entre clips

- [ ] 7.1 Implementar en el modelo agregar/cambiar/quitar transición con solapamiento y desplazamiento de los clips siguientes, validación de contigüidad y duración máxima, y eliminación automática al invalidarse; verificar con tests "Transición de 1 segundo", "Clips no contiguos", "Duración excesiva", "Cambiar la duración", "Quitar la transición" y "Eliminar un clip con transición"
- [ ] 7.2 Extender `FilterGraphBuilder` con segmentos unidos por `xfade` (mapeo de estilos de design D4), `acrossfade` para el audio vinculado con `anullsrc` en el lado sin audio, y arranque en T dentro de una transición (retroceso a T0 y descarte); verificar con tests de cadena y un test de integración que la salida dura `la + lb − D`
- [ ] 7.3 Dibujar las transiciones en `CTimelineWnd` (rombo seleccionable, "Agregar transición" en el menú contextual entre dos clips) y diseñar `IDD_PROP_TRANSICION` (estilo, dirección, duración, quitar); verificar a mano "Elegir barrido", "Elegir círculo", "Cambiar el estilo", "Audio en ambos clips" y "Solo un clip con audio"

## 8. Fase 8 — Overlay picture-in-picture

- [ ] 8.1 Implementar `PipTransform` con valores por defecto (0.65, 0.65, 0.3, 1.0) y límites en el modelo; verificar con un test que un clip nuevo en V2 tiene esos valores y que se limitan los valores fuera de rango
- [ ] 8.2 Extender `FilterGraphBuilder` con la composición abajo→arriba (scale por fracción del lienzo, `format=yuva420p`, `colorchannelmixer=aa`, `overlay` con `eof_action=pass`, efectos/transiciones antes del overlay); verificar con un test de integración que un PiP que empieza en 3 s no congela el fondo antes de 3 s (riesgo de design) y con tests de cadena
- [ ] 8.3 Añadir a `IDD_PROP_CLIP` una sección PiP (sliders de x, y, escala y opacidad, visible solo en pistas superiores a V1); verificar a mano "PiP por defecto", "Cambiar opacidad", "Cambiar posición y escala", "Clip parcialmente fuera", "Solo pista superior" y "Efecto en el PiP"

## 9. Fase 9 — Exportación

- [ ] 9.1 Implementar `EncoderCatalog` (prueba con `avcodec_open2` de h264_mf, hevc_mf, mpeg4, aac, aac_mf y compatibilidad con `avformat_query_codec` para mp4/mkv/mov); verificar con un test que al menos `mpeg4` + `aac` aparecen como válidos en los tres contenedores
- [ ] 9.2 Implementar `ExportJob` en `std::jthread` (grafo con T=0 y resolución de salida con scale+pad, pix_fmt del encoder, bitrate de video/audio, encoding + mux, progreso atómico, cancelación con `stop_token`, borrado del parcial en fallo o cancelación); verificar con un test de integración que exporta 5 s a mkv/mpeg4/aac y `MediaProbe` reporta duración, resolución y streams esperados
- [ ] 9.3 Implementar en el controller la validación de ruta de salida (rechazo si coincide con una fuente, confirmación antes de sobrescribir), resolución par entre 16 y 7680 y bloqueo de la edición durante la exportación; verificar con tests "Salida igual a una fuente" y "Resolución impar"
- [ ] 9.4 Diseñar `IDD_EXPORTAR` (contenedor, códecs filtrados, bitrates con valores por defecto 5000/192 kbps, presets 480p/720p/1080p/personalizada, `CProgressCtrl` actualizado por `SetTimer`, Cancelar, aviso final por `WM_APP_EXPORT_DONE`); verificar a mano "Elegir mkv", "Preset 720p", "Progreso visible", "Cancelar", "Exportación terminada" y "Encoder falla al iniciar"
- [ ] 9.5 Verificar `h264_mf` en el equipo de desarrollo exportando a mp4 con 2000 kbps y comprobar con `ffprobe.exe` que el bitrate medio es cercano y que la reproducción en un reproductor externo es correcta; documentar en el README la tolerancia observada

## 10. Persistencia del proyecto (JSON)

- [ ] 10.1 Implementar `ProjectSerializer` (formatVersion 1, todas las entidades, validación completa antes de reemplazar el proyecto); verificar con tests "Guardar", "Ida y vuelta" y "JSON corrupto"
- [ ] 10.2 Implementar en el controller la revalidación de medios al cargar (clips offline renderizados en negro y en silencio, aviso con rutas faltantes); verificar con un test "Archivo movido"
- [ ] 10.3 Conectar Archivo → Nuevo/Abrir/Guardar/Guardar como (`CFileDialog` con filtro `.json`, Ctrl+S) y el aviso de cambios sin guardar (Sí/No/Cancelar) en `OnClose`, Nuevo y Abrir; verificar a mano "Cerrar con cambios"

## 11. Integración final

- [ ] 11.1 Recorrer de punta a punta un proyecto con tres videos, un split, un trim, audio separado, música, efectos, una transición círculo y un PiP; guardar, reabrir y exportar a mp4, mkv y mov; verificar que el resultado coincide con el preview y que los medios fuente no cambiaron
- [ ] 11.2 Revisar que cada archivo tiene comentarios de cabecera que explican su responsabilidad y su capa, y que el README describe cómo abrir la solución en VS 2026, cómo cambiar `FFmpegRoot` y cómo correr los tests; verificar copiando el repositorio a una carpeta limpia y compilando con los pasos del README

## Workflow follow-up

- Archivar el change (`/opsx:archive`) cuando todas las fases estén implementadas y verificadas.
