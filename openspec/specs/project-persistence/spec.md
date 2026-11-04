# project-persistence Specification

## Purpose

Guardar el proyecto completo en un archivo JSON legible y volver a cargarlo después, de modo que se reconstruya exactamente el mismo timeline sin copiar ni modificar los medios fuente.

## Requirements

### Requirement: Guardar proyecto en JSON
El usuario MUST poder guardar el proyecto en un archivo JSON. El archivo incluye un número de versión de formato, la configuración del lienzo (resolución y fps), las pistas (tipo, nombre, mute y, para las pistas de audio de video, a qué pista de video pertenecen), los clips (ruta del medio, in, out, posición, clip vinculado, efectos con parámetros y transformación PiP), las transiciones (clips involucrados, estilo y duración) y la última configuración de exportación.

#### Scenario: Guardar
- **WHEN** el usuario guarda un proyecto con dos pistas de video, efectos y una transición
- **THEN** se crea un archivo .json legible que contiene toda esa información y ninguna copia de los medios

### Requirement: Cargar proyecto desde JSON
El usuario MUST poder abrir un archivo de proyecto JSON y obtener el mismo timeline que se guardó: las mismas pistas, clips, tiempos, efectos, transiciones y configuración.

#### Scenario: Ida y vuelta
- **WHEN** el usuario guarda un proyecto, lo cierra y lo vuelve a abrir
- **THEN** el timeline, el preview y la configuración de exportación son idénticos a los de antes de guardar

### Requirement: Medios faltantes al cargar
Si un medio referenciado por el proyecto ya no existe o no se puede abrir, el sistema MUST cargar el resto del proyecto, marcar los clips de ese medio como "offline" (se muestran en el timeline y se renderizan en negro y en silencio) e informar al usuario qué archivos faltan.

#### Scenario: Archivo movido
- **WHEN** el usuario abre un proyecto cuyo video B.mp4 fue borrado del disco
- **THEN** el proyecto se abre, los clips de B aparecen marcados como offline y se muestra un aviso con la ruta de B.mp4

### Requirement: Archivos de proyecto inválidos
Si el archivo JSON está mal formado, le faltan campos obligatorios o tiene una versión de formato no soportada, el sistema MUST rechazar la carga con un mensaje claro y conservar el proyecto que estaba abierto.

#### Scenario: JSON corrupto
- **WHEN** el usuario abre un archivo .json truncado
- **THEN** se muestra un error de formato y el proyecto actual no cambia

### Requirement: Aviso de cambios sin guardar
El sistema MUST avisar al usuario antes de cerrar la aplicación, crear un proyecto nuevo o abrir otro si hay cambios sin guardar.

#### Scenario: Cerrar con cambios
- **WHEN** el usuario cierra la ventana después de modificar el timeline sin guardar
- **THEN** se le pregunta si desea guardar, descartar o cancelar el cierre
