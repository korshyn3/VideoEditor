## Purpose

Definir la estructura del timeline multipista y las operaciones de edición de clips (colocar, mover, dividir, recortar y eliminar), que siempre trabajan sobre referencias a los archivos fuente sin modificarlos.

## ADDED Requirements

### Requirement: Estructura del timeline
El proyecto MUST tener un timeline con una o más pistas de video, una pista de audio de video por cada pista de video y cero o más pistas de audio generales (ver specs/audio-tracks). Cada clip referencia un medio fuente, un punto de entrada (in) y uno de salida (out) dentro de ese medio, y una posición de inicio en el timeline. Un proyecto nuevo empieza con una pista de video, su pista de audio y una pista de audio general.

#### Scenario: Proyecto nuevo
- **WHEN** el usuario crea un proyecto nuevo
- **THEN** el timeline tiene una pista de video (V1), su pista de audio (Audio V1), una pista de audio general (A1) y ningún clip

#### Scenario: Agregar pistas
- **WHEN** el usuario agrega una pista de video o de audio
- **THEN** la nueva pista aparece en el timeline vacía, con nombre correlativo (V2, A2, ...); una pista de video nueva trae además su pista de audio (Audio V2)

### Requirement: Configuración del lienzo del proyecto
El proyecto MUST tener una resolución y unos fps de salida. Al importar el primer video en un proyecto vacío, toma la resolución y los fps de ese video. Después, el usuario los puede cambiar.

#### Scenario: Primer video define el lienzo
- **WHEN** el usuario importa un video 1280x720 a 25 fps en un proyecto sin clips
- **THEN** el proyecto queda configurado en 1280x720 a 25 fps

#### Scenario: Video de distinta resolución
- **WHEN** se agrega a la pista V1 un clip cuya resolución difiere de la del proyecto
- **THEN** el clip se escala para caber en el lienzo conservando su relación de aspecto, con barras negras donde haga falta

### Requirement: Colocación de videos importados en orden
Al importar uno o varios videos, el sistema MUST colocarlos en la pista de video seleccionada, uno a continuación del otro y en el orden de selección, empezando al final del último clip de esa pista. Cada clip abarca el archivo completo. Los videos con audio crean además su clip de audio vinculado (specs/audio-tracks).

#### Scenario: Importar tres videos
- **WHEN** el usuario importa A.mp4 (10 s), B.mp4 (5 s) y C.mp4 (8 s) en una pista V1 vacía
- **THEN** A ocupa 0–10 s, B ocupa 10–15 s y C ocupa 15–23 s

### Requirement: Mover clips
El usuario MUST poder arrastrar un clip a otra posición de la misma pista o a otra pista del mismo tipo. Un clip no puede solaparse con otro de la misma pista: si el destino se solapa, el movimiento se rechaza y el clip vuelve a su lugar. Un clip de audio vinculado se puede mover solo en el tiempo (su pista la decide la de su video); el movimiento arrastra al clip vinculado.

#### Scenario: Mover a un hueco libre
- **WHEN** el usuario arrastra un clip a una zona vacía de la pista V2
- **THEN** el clip queda en V2 en la nueva posición

#### Scenario: Movimiento con solapamiento
- **WHEN** el usuario suelta un clip encima de otro clip de la misma pista
- **THEN** el movimiento se rechaza y el clip conserva su posición original

### Requirement: Dividir clip en el cursor (split)
El usuario MUST poder dividir el clip seleccionado en la posición del cursor de reproducción. Resultan dos clips contiguos que juntos cubren el mismo rango del medio fuente y conservan los efectos del original. Si el clip tiene un clip vinculado, también se divide. Si el cursor no está estrictamente dentro del clip, la operación no hace nada.

#### Scenario: Split en el medio
- **WHEN** el cursor está en 4 s y el usuario divide un clip que va de 0 s a 10 s (fuente 0–10 s)
- **THEN** quedan dos clips: 0–4 s (fuente 0–4 s) y 4–10 s (fuente 4–10 s)

#### Scenario: Cursor fuera del clip
- **WHEN** el cursor está fuera del clip seleccionado o justo en uno de sus bordes
- **THEN** no se realiza ninguna división

### Requirement: Recortar inicio y fin (trim)
El usuario MUST poder mover el punto de entrada o de salida de un clip, ya sea arrastrando sus bordes o con valores numéricos. El nuevo rango debe quedar dentro de la duración del medio fuente, durar al menos un fotograma y no solaparse con clips vecinos (tampoco los vecinos del clip vinculado, que recibe el mismo recorte).

#### Scenario: Recortar el inicio
- **WHEN** el usuario arrastra el borde izquierdo de un clip 2 s hacia la derecha
- **THEN** el clip empieza 2 s más tarde en el timeline y su punto de entrada en la fuente avanza 2 s

#### Scenario: Extender más allá de la fuente
- **WHEN** el usuario intenta extender el punto de salida más allá del final del archivo fuente
- **THEN** el punto de salida queda limitado al final del archivo

### Requirement: Eliminar clips y segmentos
El usuario MUST poder eliminar el clip seleccionado de dos formas: dejando un hueco, o eliminando con desplazamiento (ripple), donde los clips siguientes de la misma pista se corren a la izquierda para cerrar el hueco. Para quitar un segmento intermedio, se hacen dos splits y se elimina el clip del medio. Eliminar un clip vinculado elimina también su clip vinculado, y el ripple se aplica en las dos pistas.

#### Scenario: Eliminar dejando hueco
- **WHEN** el usuario elimina el clip B (10–15 s) de V1 con eliminación normal
- **THEN** B desaparece y el clip C sigue en 15 s, dejando un hueco de 10 a 15 s que se renderiza negro y en silencio

#### Scenario: Eliminar con ripple
- **WHEN** el usuario elimina el clip B (10–15 s) de V1 con ripple
- **THEN** B desaparece y C pasa a empezar en 10 s

#### Scenario: Eliminar un segmento intermedio
- **WHEN** el usuario divide un clip en 3 s y en 6 s y elimina el trozo 3–6 s con ripple
- **THEN** el timeline reproduce la fuente 0–3 s seguida inmediatamente de 6 s en adelante

### Requirement: Eliminar pistas
El usuario MUST poder eliminar una pista junto con todos sus clips. Siempre debe quedar al menos una pista de video. Eliminar una pista de video elimina también su pista de audio de video; esta no se puede eliminar sola. Si un clip eliminado estaba vinculado con un clip de otra pista, ese otro clip queda desvinculado.

#### Scenario: Eliminar la única pista de video
- **WHEN** el usuario intenta eliminar la última pista de video que queda
- **THEN** la operación se rechaza con un mensaje
