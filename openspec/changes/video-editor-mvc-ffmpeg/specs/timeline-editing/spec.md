## Purpose

Definir la estructura del timeline multipista y las operaciones de edición de clips (colocar, mover, dividir, recortar y eliminar), que siempre trabajan sobre referencias a los archivos fuente sin modificarlos.

## ADDED Requirements

### Requirement: Estructura del timeline
El proyecto MUST tener un timeline con una o más pistas de video, una pista de audio de video por cada pista de video y cero o más pistas de audio generales (ver specs/audio-tracks). Cada clip referencia un medio fuente, un punto de entrada (in) y uno de salida (out) dentro de ese medio, y una posición de inicio en el timeline. Un proyecto nuevo empieza con una pista de video, su pista de audio y una pista de audio general.

#### Scenario: Proyecto nuevo
- **WHEN** el usuario crea un proyecto nuevo
- **THEN** el timeline tiene una pista de video (V1), su pista de audio (AV1), una pista de audio general (A1) y ningún clip

#### Scenario: Agregar pistas
- **WHEN** el usuario agrega una pista de video o de audio
- **THEN** la nueva pista aparece en el timeline vacía, con nombre correlativo (V2, A2, ...); una pista de video nueva trae además su pista de audio (AV2)

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

### Requirement: Selección múltiple
El usuario MUST poder seleccionar varios clips a la vez, de cualquier pista: Ctrl+clic agrega un clip a la selección o lo quita, y un clic sin Ctrl deja seleccionado solo ese clip. Dividir y eliminar se aplican a todos los clips seleccionados; recortar y desvincular solo están disponibles con un único clip seleccionado. Si una acción no puede aplicarse a alguno de los clips, no se aplica a ninguno.

#### Scenario: Seleccionar video y música
- **WHEN** el usuario hace clic en un clip de V1 y Ctrl+clic en un clip de A1
- **THEN** los dos clips quedan seleccionados

#### Scenario: Dividir la selección
- **WHEN** hay dos clips seleccionados, el cursor está dentro de uno de ellos y fuera del otro, y el usuario divide
- **THEN** se divide solo el clip que está bajo el cursor

#### Scenario: Eliminar la selección
- **WHEN** hay tres clips seleccionados en distintas pistas y el usuario los elimina
- **THEN** desaparecen los tres (y sus clips vinculados)

#### Scenario: Recortar con varios clips seleccionados
- **WHEN** hay más de un clip seleccionado
- **THEN** recortar y desvincular no están disponibles

### Requirement: Mover varios clips juntos
Al arrastrar un clip que forma parte de una selección múltiple, el sistema MUST mover todos los clips seleccionados (y sus clips vinculados) el mismo tiempo, cada uno en su pista. Con varios clips seleccionados no se cambia de pista. Si alguno se solaparía con otro clip, el movimiento se rechaza y ninguno se mueve.

#### Scenario: Mover la selección
- **WHEN** el usuario selecciona un video en V1 y una música en A1 y arrastra uno de ellos 3 s a la derecha
- **THEN** los dos (y el audio vinculado del video) quedan 3 s más tarde, cada uno en su pista

#### Scenario: Movimiento de la selección con solapamiento
- **WHEN** al mover la selección uno de los clips quedaría encima de otro clip
- **THEN** el movimiento se rechaza y todos los clips conservan su posición

### Requirement: Mover sobrescribiendo (Shift)
Si el usuario mantiene Shift al soltar un movimiento (de uno o de varios clips), el sistema MUST sobrescribir: los clips que quedan debajo de los clips movidos se recortan para dejarles lugar. Un clip tapado por completo se elimina; si se tapa su inicio o su final, se recorta esa parte; si los clips movidos caen en su medio, se divide en dos y se quita la parte tapada. Si el clip de debajo está vinculado, su clip vinculado recibe el mismo corte.

#### Scenario: Sobrescribir el final de un clip
- **WHEN** A ocupa 0–10 s, el usuario arrastra B (5 s) con Shift hasta empezar en 8 s
- **THEN** B queda en 8–13 s y A queda recortado a 0–8 s

#### Scenario: Sobrescribir el medio de un clip
- **WHEN** A ocupa 0–10 s y el usuario suelta con Shift un clip de 2 s en 4 s
- **THEN** A queda dividido en 0–4 s y 6–10 s, con el clip movido en 4–6 s

#### Scenario: Tapar un clip completo
- **WHEN** el clip movido con Shift cubre por completo a otro clip de la pista
- **THEN** el clip tapado se elimina

#### Scenario: Sobrescribir un video con audio vinculado
- **WHEN** el usuario suelta con Shift un clip sin audio sobre el tramo 4–6 s de un video con audio vinculado
- **THEN** el video y su audio quedan los dos sin el tramo 4–6 s

### Requirement: Pegado de clips según el zoom
Al mover un clip (o una selección) o al recortar uno de sus bordes con el mouse, si un borde queda cerca del borde de otro clip o del inicio del timeline, el sistema MUST pegarlo a ese punto y marcarlo con una línea vertical durante el arrastre. La cercanía se mide en píxeles de pantalla (8 px), de modo que en tiempo depende del zoom. Tienen prioridad las pistas en las que quedarían los clips movidos y sus clips vinculados; solo si ahí no hay ningún punto cerca, se alinea con los clips de las demás pistas.

#### Scenario: Pegar con poco zoom
- **WHEN** el zoom es de 50 px/s, A termina en 5 s y el usuario arrastra B hasta empezar en 5.1 s (a 5 px)
- **THEN** B queda empezando exactamente en 5 s

#### Scenario: No pegar con mucho zoom
- **WHEN** el zoom es de 200 px/s, A termina en 5 s y el usuario arrastra B hasta empezar en 5.1 s (a 20 px)
- **THEN** B queda empezando en 5.1 s

#### Scenario: Alinear con otra pista
- **WHEN** un video de V1 empieza en 5 s, A1 no tiene clips cerca y el usuario arrastra música a menos de 8 px de los 5 s
- **THEN** la música queda empezando exactamente en 5 s

#### Scenario: La pista propia tiene prioridad
- **WHEN** A termina en 5 s en V1, un clip de V2 empieza en 5.2 s y el usuario arrastra B por V1 a menos de 8 px de los dos puntos
- **THEN** B queda pegado a A aunque el borde de V2 esté más cerca, y no queda encima de A

#### Scenario: Pegar al recortar
- **WHEN** el usuario arrastra el borde derecho de un clip a menos de 8 px del inicio del clip siguiente y la fuente alcanza
- **THEN** el clip queda terminando exactamente donde empieza el siguiente

#### Scenario: Pegar al inicio del timeline
- **WHEN** el usuario arrastra un clip a menos de 8 px del instante 0
- **THEN** el clip queda empezando en 0

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
