## Purpose

Permitir transiciones visuales y de audio entre dos clips consecutivos de una misma pista de video, eligiendo el estilo y la duración, y poder cambiarlas o quitarlas después.

## ADDED Requirements

### Requirement: Agregar transición entre clips consecutivos
El usuario MUST poder agregar una transición entre dos clips de la misma pista de video que estén contiguos (sin hueco). Durante la transición ambos clips se solapan durante D segundos, por lo que el segundo clip y los siguientes de esa pista se adelantan D segundos y la pista se acorta en D.

#### Scenario: Transición de 1 segundo
- **WHEN** A ocupa 0–10 s, B ocupa 10–15 s y el usuario agrega una transición de 1 s entre A y B
- **THEN** B pasa a empezar en 9 s, la transición ocurre de 9 a 10 s y la pista termina en 14 s

#### Scenario: Clips no contiguos
- **WHEN** hay un hueco entre A y B y el usuario intenta agregar una transición
- **THEN** la acción se rechaza con un mensaje indicando que los clips deben estar contiguos

### Requirement: Estilos de transición
El sistema MUST ofrecer al menos los estilos fundido (dissolve), barrido (wipe, izquierda/derecha), deslizamiento (slide, izquierda/derecha) y círculo (apertura circular). El preview y la exportación muestran el mismo estilo.

#### Scenario: Elegir barrido
- **WHEN** el usuario elige el estilo "barrido" para una transición
- **THEN** durante la transición la imagen de B reemplaza a la de A con un borde que recorre la pantalla horizontalmente

#### Scenario: Elegir círculo
- **WHEN** el usuario elige el estilo "círculo"
- **THEN** la imagen de B aparece desde el centro en forma de círculo que crece hasta cubrir el cuadro

### Requirement: Duración válida de la transición
La duración MUST indicarse en segundos, ser mayor que 0 y no superar la duración de ninguno de los dos clips involucrados (descontando los fades y otras transiciones que ya usan esos clips).

#### Scenario: Duración excesiva
- **WHEN** el usuario pide una transición de 6 s entre un clip de 10 s y uno de 5 s
- **THEN** el sistema la rechaza o la limita al máximo permitido e informa al usuario

### Requirement: Crossfade de audio en la transición
Si alguno de los dos clips tiene audio vinculado, el sistema MUST hacer crossfade de ese audio durante la transición: el audio de A baja mientras el de B sube, con la misma duración. Un clip cuyo audio ya se separó no participa (la pista de audio separada no se ve afectada).

#### Scenario: Audio en ambos clips
- **WHEN** A y B tienen audio vinculado y hay una transición de 2 s entre ellos
- **THEN** durante esos 2 s el audio de A se desvanece mientras el de B aparece, sin cortes bruscos

#### Scenario: Solo un clip con audio
- **WHEN** solo A tiene audio vinculado
- **THEN** el audio de A se desvanece durante la transición y B entra sin audio

### Requirement: Cambiar o quitar una transición
El usuario MUST poder seleccionar una transición existente y cambiar su estilo o duración, o quitarla. Al quitarla, el solapamiento desaparece: el segundo clip y los siguientes de la pista vuelven a correrse D segundos a la derecha.

#### Scenario: Cambiar el estilo
- **WHEN** el usuario cambia una transición de "fundido" a "deslizamiento"
- **THEN** el preview muestra el deslizamiento y la posición de los clips no cambia

#### Scenario: Cambiar la duración
- **WHEN** el usuario cambia una transición de 1 s a 2 s
- **THEN** el segundo clip y los siguientes de la pista se adelantan 1 s más

#### Scenario: Quitar la transición
- **WHEN** el usuario quita una transición de 1 s entre A (0–10 s) y B (9–14 s)
- **THEN** B vuelve a ocupar 10–15 s y el corte entre A y B es directo

### Requirement: Transiciones al editar clips
Si un clip involucrado en una transición se elimina, se mueve fuera de la contigüidad o se recorta de modo que la transición ya no sea válida, el sistema MUST quitar esa transición automáticamente.

#### Scenario: Eliminar un clip con transición
- **WHEN** el usuario elimina el clip B, que tiene una transición con A
- **THEN** la transición A–B desaparece junto con B
