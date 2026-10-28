## Purpose

Gestionar el audio del proyecto: el audio vinculado a los clips de video, las pistas de audio independientes, la importación de archivos de audio y la separación del audio de un clip de video.

## ADDED Requirements

### Requirement: Audio vinculado de los clips de video
Un clip de video cuyo medio tiene audio MUST reproducir y exportar ese audio sincronizado con la imagen, usando el mismo rango in/out, mientras el audio no se haya separado.

#### Scenario: Split conserva el audio vinculado
- **WHEN** el usuario divide un clip de video con audio vinculado
- **THEN** cada mitad reproduce el audio correspondiente a su rango

### Requirement: Separar audio de un clip de video
El usuario MUST poder separar el audio de un clip de video. Se crea un clip de audio con el mismo medio, rango y posición en una pista de audio libre en ese rango (o en una pista nueva si no hay ninguna), y el clip de video queda sin audio vinculado.

#### Scenario: Separar audio
- **WHEN** el usuario separa el audio de un clip de video que ocupa 5–12 s
- **THEN** aparece un clip de audio de 5–12 s en una pista de audio, y el clip de video ya no aporta audio

#### Scenario: Clip sin audio
- **WHEN** el usuario intenta separar el audio de un clip cuyo medio no tiene audio
- **THEN** la acción se rechaza con un mensaje y nada cambia

### Requirement: Clips de audio independientes
Un clip de audio separado o importado MUST comportarse como cualquier clip: se puede mover, dividir, recortar y borrar sin afectar al clip de video de origen.

#### Scenario: Mover el audio separado
- **WHEN** el usuario mueve el clip de audio separado 2 s hacia la derecha
- **THEN** el audio suena 2 s después y la imagen del clip de video no cambia

#### Scenario: Borrar el audio separado
- **WHEN** el usuario borra el clip de audio separado
- **THEN** ese tramo del video queda en silencio

### Requirement: Importar archivos de audio
El usuario MUST poder importar archivos de audio (mp3, wav, aac) a la pista de audio seleccionada. Se colocan en orden a continuación del último clip de esa pista.

#### Scenario: Importar música
- **WHEN** el usuario importa musica.mp3 (60 s) en la pista A2 vacía
- **THEN** aparece un clip de audio de 0–60 s en A2

#### Scenario: Importar video en pista de audio
- **WHEN** el usuario intenta colocar un archivo con video en una pista de audio
- **THEN** el sistema coloca únicamente su audio como clip de audio, o rechaza la acción si el archivo no tiene audio

### Requirement: Mezcla de pistas de audio
El sistema MUST mezclar (sumar) en la reproducción y en la exportación todo el audio que suena a la vez: el audio vinculado de los clips de video y todas las pistas de audio. Cada pista de audio puede silenciarse (mute).

#### Scenario: Música sobre video con audio
- **WHEN** un clip de video con audio vinculado y un clip de música suenan en el mismo instante
- **THEN** ambos se oyen mezclados

#### Scenario: Pista silenciada
- **WHEN** el usuario silencia la pista A2
- **THEN** sus clips no se oyen en el preview ni en la exportación
