# video-export Specification

## Purpose

Renderizar el timeline completo a un archivo de video nuevo con el contenedor, los códecs, el bitrate y la resolución elegidos, en segundo plano, mostrando el progreso y permitiendo cancelar.

## Requirements

### Requirement: Contenedores de salida
El sistema MUST permitir exportar en los contenedores mp4, mkv y mov. La extensión del archivo de salida corresponde al contenedor elegido.

#### Scenario: Elegir mkv
- **WHEN** el usuario elige el contenedor mkv y el nombre "salida"
- **THEN** el archivo exportado es "salida.mkv" en formato Matroska

### Requirement: Códecs disponibles según el sistema
El diálogo de exportación MUST ofrecer solo los códecs de video y audio que tienen encoder disponible en la instalación actual y que el contenedor elegido admite. Al menos se consideran H.264, HEVC y MPEG-4 Part 2 para video, y AAC para audio. Si no hay ninguna combinación válida, se informa al usuario.

#### Scenario: Encoders disponibles
- **WHEN** el usuario abre el diálogo de exportación con mp4 seleccionado
- **THEN** la lista de códecs de video muestra solo los que pueden codificarse en ese equipo, por ejemplo H.264 y MPEG-4

#### Scenario: Encoder falla al iniciar
- **WHEN** el encoder elegido no puede inicializarse (por ejemplo, el hardware no lo soporta)
- **THEN** la exportación termina con un mensaje que nombra el códec y la causa, y no queda archivo de salida a medio escribir

### Requirement: Bitrate configurable
El usuario MUST poder indicar el bitrate de video en kbps (de 100 a 100000) y el de audio en kbps (de 32 a 512). Los valores por defecto son 5000 kbps para video y 192 kbps para audio.

#### Scenario: Bitrate personalizado
- **WHEN** el usuario exporta con 2000 kbps de video
- **THEN** el archivo resultante tiene un bitrate medio de video cercano a 2000 kbps

### Requirement: Resolución de salida
El usuario MUST poder elegir la resolución de salida entre los presets 480p (854x480), 720p (1280x720) y 1080p (1920x1080), o una resolución personalizada de ancho y alto pares entre 16 y 7680. El cuadro del proyecto se escala a esa resolución conservando la relación de aspecto, con barras negras si hace falta.

#### Scenario: Preset 720p
- **WHEN** el usuario exporta un proyecto 1920x1080 con el preset 720p
- **THEN** el video resultante mide 1280x720

#### Scenario: Resolución impar
- **WHEN** el usuario introduce 1279x719 como resolución personalizada
- **THEN** el sistema la ajusta a valores pares o rechaza el valor con un mensaje

### Requirement: Contenido exportado
El archivo exportado MUST contener el timeline completo, desde 0 hasta el final del último clip de cualquier pista, con los fps del proyecto, todos los efectos, las transiciones, la composición PiP y el audio mezclado (estéreo).

#### Scenario: Exportación completa
- **WHEN** el usuario exporta un timeline de 30 s con una transición, un PiP y música
- **THEN** el archivo dura 30 s y al reproducirlo en un reproductor externo se ven la transición y el PiP y se oye la música mezclada

### Requirement: Exportación en segundo plano con progreso
La exportación MUST ejecutarse sin bloquear la interfaz, mostrando el porcentaje completado. Mientras exporta, las acciones que modifican el proyecto quedan deshabilitadas.

#### Scenario: Progreso visible
- **WHEN** se está exportando un timeline de 60 s y se han codificado 15 s
- **THEN** la barra de progreso muestra aproximadamente un 25 % y la ventana sigue respondiendo

#### Scenario: Exportación terminada
- **WHEN** la exportación termina correctamente
- **THEN** el sistema informa éxito con la ruta del archivo y vuelve a habilitar la edición

### Requirement: Cancelar exportación
El usuario MUST poder cancelar una exportación en curso. Al cancelar, el proceso se detiene en poco tiempo, el archivo parcial se elimina y el proyecto queda intacto.

#### Scenario: Cancelar
- **WHEN** el usuario pulsa "Cancelar" con la exportación al 40 %
- **THEN** la exportación se detiene, el archivo parcial se borra y se informa que fue cancelada

### Requirement: Protección de archivos fuente y destino
El sistema MUST rechazar una ruta de salida que coincida con algún archivo fuente del proyecto, y MUST pedir confirmación antes de sobrescribir otro archivo existente.

#### Scenario: Salida igual a una fuente
- **WHEN** el usuario elige como salida la ruta de uno de los videos importados
- **THEN** la exportación se rechaza con un mensaje y el archivo fuente no se toca
