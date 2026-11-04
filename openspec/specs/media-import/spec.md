# media-import Specification

## Purpose

Abrir y analizar archivos multimedia (video y audio) para conocer sus características antes de usarlos en el timeline, y rechazar con mensajes claros los archivos que no se pueden usar.

## Requirements

### Requirement: Análisis de archivos multimedia
El sistema MUST abrir un archivo multimedia en modo solo lectura y obtener su información: duración, presencia de streams de video y audio, resolución, fps, códec de video, códec de audio, frecuencia de muestreo y número de canales.

#### Scenario: Video con audio válido
- **WHEN** el usuario importa un archivo mp4 con un stream H.264 y un stream AAC
- **THEN** el sistema registra el medio con duración, resolución, fps y códecs, e indica que tiene video y audio

#### Scenario: Video sin audio
- **WHEN** el usuario importa un video que no tiene stream de audio
- **THEN** el sistema registra el medio como solo video, sin audio asociado

#### Scenario: Ver la información de un medio
- **WHEN** el usuario selecciona un medio importado o un clip
- **THEN** el panel de propiedades muestra la ruta, la duración, la resolución, los fps y los códecs del archivo fuente

### Requirement: Formatos de audio soportados
El sistema MUST aceptar como archivos de solo audio al menos mp3, wav y aac.

#### Scenario: Importar un mp3
- **WHEN** el usuario importa un archivo .mp3 válido
- **THEN** el sistema lo registra como medio de solo audio con su duración, frecuencia de muestreo y canales

### Requirement: Rechazo de archivos inválidos
El sistema MUST rechazar los archivos que no existen, que no se pueden abrir o leer, que no contienen streams de video ni de audio, o cuyo códec no tiene decodificador disponible. En esos casos muestra un mensaje que nombra el archivo y la causa, y no modifica el proyecto.

#### Scenario: Archivo corrupto o que no es multimedia
- **WHEN** el usuario intenta importar un archivo de texto renombrado como .mp4
- **THEN** el sistema muestra un error indicando que el archivo no es un medio válido y el timeline no cambia

#### Scenario: Códec sin decodificador
- **WHEN** el usuario importa un archivo cuyo stream de video usa un códec para el que no hay decodificador
- **THEN** el sistema muestra un error indicando el códec no soportado y no agrega el clip

#### Scenario: Importación múltiple con un archivo inválido
- **WHEN** el usuario importa tres archivos y uno de ellos es inválido
- **THEN** los dos válidos se importan y se informa el error del inválido

### Requirement: Archivos originales intactos
El sistema MUST NOT escribir, renombrar ni borrar ningún archivo fuente importado durante ninguna operación.

#### Scenario: Edición no destructiva
- **WHEN** el usuario importa, corta, aplica efectos y exporta un proyecto
- **THEN** los archivos fuente conservan exactamente el mismo contenido y la misma fecha de modificación que antes
