## Purpose

Gestionar el audio del proyecto: las pistas de audio de video (una por cada pista de video, con el audio vinculado de sus clips), las pistas de audio generales, la importación de archivos de audio, la desvinculación del audio de un clip de video y la mezcla.

## ADDED Requirements

### Requirement: Tipos de pista de audio
El proyecto MUST distinguir dos tipos de pista de audio. Cada pista de video tiene exactamente una **pista de audio de video** ("AVn"), que se crea y se elimina junto con ella y recibe automáticamente el audio de sus clips. Las **pistas de audio generales** (A1, A2, ...) nunca reciben audio de video automáticamente: un proyecto nuevo tiene una, y toda pista de audio que agrega el usuario es general.

#### Scenario: Proyecto nuevo
- **WHEN** el usuario crea un proyecto nuevo
- **THEN** el timeline tiene V1, AV1 y A1, sin clips

#### Scenario: Agregar una pista de video
- **WHEN** el usuario agrega la pista de video V2
- **THEN** también aparece la pista AV2, vacía

#### Scenario: Agregar una pista de audio
- **WHEN** el usuario agrega una pista de audio
- **THEN** aparece una pista de audio general con el siguiente nombre (A2, A3, ...)

#### Scenario: Eliminar una pista de audio de video
- **WHEN** el usuario intenta eliminar la pista AV1
- **THEN** la operación se rechaza con un mensaje: esa pista se elimina solo junto con V1

### Requirement: Orden de las pistas en el timeline
El timeline MUST mostrar primero las pistas de video (la más alta arriba), después las pistas de audio de video (AV1, AV2, ...) y al final las pistas de audio generales.

#### Scenario: Orden con dos pistas de video
- **WHEN** el proyecto tiene V1, V2 y las pistas generales A1 y A2
- **THEN** de arriba a abajo se ven V2, V1, AV1, AV2, A1 y A2

### Requirement: Audio vinculado de los clips de video
Al colocar en una pista de video un medio que tiene audio, el sistema MUST crear además un clip de audio con el mismo medio, rango in/out y posición en la pista de audio de esa pista de video, vinculado al clip de video. Mientras están vinculados, los dos clips tienen siempre la misma posición y el mismo rango, y el clip de audio permanece en la pista de audio de la pista de video donde está el video. El clip de video no produce sonido por sí mismo: todo el audio sale de las pistas de audio.

#### Scenario: Importar un video con audio
- **WHEN** el usuario importa A.mp4 (10 s, con audio) en V1 vacía
- **THEN** aparecen un clip de video de 0–10 s en V1 y un clip de audio de 0–10 s en AV1, vinculados

#### Scenario: Importar un video sin audio
- **WHEN** el usuario importa un video sin stream de audio en V1
- **THEN** solo aparece el clip de video, sin clip de audio

### Requirement: Operaciones sobre clips vinculados
Mover, dividir, recortar y eliminar un clip vinculado MUST aplicarse también a su clip vinculado, se actúe sobre el clip de video o sobre el de audio. Si la operación no es válida para alguno de los dos (por ejemplo, porque el audio se solaparía con otro clip de su pista), se rechaza entera y ninguno cambia.

#### Scenario: Mover el video a otra pista
- **WHEN** el usuario mueve un clip de video vinculado de V1 a V2 y 3 s a la derecha
- **THEN** su clip de audio pasa de AV1 a AV2 y también se corre 3 s

#### Scenario: Mover el audio vinculado
- **WHEN** el usuario arrastra el clip de audio vinculado 2 s a la derecha
- **THEN** el clip de video también se corre 2 s y siguen sincronizados

#### Scenario: Split conserva el audio vinculado
- **WHEN** el usuario divide un clip de video con audio vinculado
- **THEN** su clip de audio también se divide en el mismo instante, cada mitad de video queda vinculada a la mitad de audio correspondiente y cada una reproduce el audio de su rango

#### Scenario: Recortar un clip vinculado
- **WHEN** el usuario recorta 2 s del inicio del clip de audio vinculado
- **THEN** el clip de video también pierde esos 2 s del inicio

#### Scenario: Eliminar un clip vinculado
- **WHEN** el usuario elimina con ripple un clip de video vinculado
- **THEN** también se elimina su clip de audio, y se cierra el hueco tanto en la pista de video como en la de audio

#### Scenario: Movimiento rechazado por el audio
- **WHEN** el usuario mueve un clip de video vinculado a una posición libre en V1, pero en AV1 hay en ese rango un clip de audio desvinculado
- **THEN** el movimiento se rechaza y los dos clips conservan su posición

### Requirement: Desvincular audio
El usuario MUST poder desvincular el audio de un clip de video. Los dos clips quedan en su lugar y pasan a ser independientes. La acción se rechaza con un mensaje si el clip no tiene audio vinculado.

#### Scenario: Desvincular audio
- **WHEN** el usuario desvincula el audio de un clip de video que ocupa 5–12 s
- **THEN** el clip de audio sigue en 5–12 s en su pista, pero ya no está vinculado: moverlo no mueve el video

#### Scenario: Clip sin audio
- **WHEN** el usuario intenta desvincular el audio de un clip que no tiene audio vinculado
- **THEN** la acción se rechaza con un mensaje y nada cambia

### Requirement: Clips de audio independientes
Un clip de audio desvinculado o importado MUST comportarse como cualquier clip: se puede mover (dentro de su pista o a cualquier otra pista de audio, de video o general), dividir, recortar y borrar sin afectar a ningún clip de video.

#### Scenario: Mover el audio desvinculado
- **WHEN** el usuario mueve el clip de audio desvinculado 2 s hacia la derecha
- **THEN** el audio suena 2 s después y la imagen del clip de video no cambia

#### Scenario: Llevar el audio a una pista general
- **WHEN** el usuario arrastra un audio desvinculado de AV1 a A1
- **THEN** el clip queda en A1 en la posición elegida

#### Scenario: Borrar el audio desvinculado
- **WHEN** el usuario borra el clip de audio desvinculado
- **THEN** ese tramo del video queda en silencio

#### Scenario: Mover un video desvinculado
- **WHEN** el usuario mueve a V2 un clip de video cuyo audio se desvinculó
- **THEN** el clip de audio no se mueve

### Requirement: Importar archivos de audio
El usuario MUST poder importar archivos de audio (mp3, wav, aac). Se colocan en la pista de audio general seleccionada (o en la primera general, que se crea si no existe), en orden a continuación del último clip de esa pista.

#### Scenario: Importar música
- **WHEN** el usuario importa musica.mp3 (60 s) con la pista general A2 vacía seleccionada
- **THEN** aparece un clip de audio de 0–60 s en A2

#### Scenario: Importar video en pista de audio
- **WHEN** el usuario importa un archivo con video teniendo seleccionada una pista de audio general
- **THEN** el sistema coloca únicamente su audio como clip de audio sin vínculo en esa pista, o rechaza la acción si el archivo no tiene audio

### Requirement: Mezcla de pistas de audio
El sistema MUST mezclar (sumar) en la reproducción y en la exportación todo el audio que suena a la vez en todas las pistas de audio, de video y generales. Cada pista de audio puede silenciarse (mute); silenciar una pista de audio de video silencia el audio de sus videos.

#### Scenario: Música sobre video con audio
- **WHEN** un clip de audio en AV1 y un clip de música en A1 suenan en el mismo instante
- **THEN** ambos se oyen mezclados

#### Scenario: Pista silenciada
- **WHEN** el usuario silencia la pista A2
- **THEN** sus clips no se oyen en el preview ni en la exportación
