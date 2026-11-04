# video-effects Specification

## Purpose

Aplicar efectos visuales por clip (escala de grises, brillo/contraste, desenfoque y fade in/out) con parámetros editables, visibles en el preview y en la exportación.

## Requirements

### Requirement: Lista de efectos por clip
Cada clip de video MUST tener una lista ordenada de efectos que el usuario puede agregar, editar, reordenar y quitar desde el panel de propiedades. Los efectos se aplican en el orden de la lista y solo afectan a ese clip.

#### Scenario: Agregar un efecto
- **WHEN** el usuario agrega "Escala de grises" al clip B
- **THEN** solo el clip B se ve en blanco y negro en el preview, y los demás clips no cambian

#### Scenario: Quitar un efecto
- **WHEN** el usuario quita un efecto de la lista del clip
- **THEN** el clip vuelve a verse sin ese efecto

### Requirement: Escala de grises
El efecto de escala de grises MUST eliminar toda la saturación de color del clip.

#### Scenario: Aplicar escala de grises
- **WHEN** se aplica escala de grises a un clip en color
- **THEN** cada fotograma del clip se ve en tonos de gris

### Requirement: Brillo y contraste
El efecto de brillo/contraste MUST tener un parámetro de brillo entre -1.0 y 1.0 (0 = sin cambio) y uno de contraste entre 0.0 y 3.0 (1 = sin cambio). Los valores fuera de rango se limitan al rango válido.

#### Scenario: Aumentar brillo
- **WHEN** el usuario fija brillo = 0.3 y contraste = 1.0
- **THEN** el clip se ve más claro en el preview y en la exportación

#### Scenario: Valor fuera de rango
- **WHEN** el usuario introduce contraste = 5
- **THEN** el valor se limita a 3.0

### Requirement: Desenfoque
El efecto de desenfoque MUST tener un parámetro de intensidad (radio) entre 0 y 50. Con 0 no hay desenfoque.

#### Scenario: Desenfocar
- **WHEN** el usuario aplica desenfoque con intensidad 10
- **THEN** el clip se ve desenfocado de forma uniforme

### Requirement: Fade in / fade out
El efecto de fade MUST permitir indicar una duración de fade in (desde negro al inicio del clip) y una de fade out (hacia negro al final del clip), en segundos. La suma de ambas no puede superar la duración del clip.

#### Scenario: Fade in de 1 segundo
- **WHEN** el usuario aplica fade in de 1 s a un clip
- **THEN** el clip empieza en negro y alcanza su imagen normal 1 s después de su inicio

#### Scenario: Fade más largo que el clip
- **WHEN** el usuario pide fade in de 4 s y fade out de 4 s en un clip de 6 s
- **THEN** el sistema rechaza el valor o lo ajusta para que la suma no supere 6 s, e informa al usuario

### Requirement: Efectos tras editar el clip
Los efectos de un clip MUST conservarse al moverlo, recortarlo o dividirlo. Al dividirlo, cada mitad recibe una copia de los efectos; el fade in queda solo en la primera mitad y el fade out solo en la segunda.

#### Scenario: Split con efectos
- **WHEN** el usuario divide un clip que tiene escala de grises y fade in/out
- **THEN** ambas mitades están en escala de grises, la primera conserva el fade in y la segunda el fade out
