## Purpose

Componer varias pistas de video en un único cuadro de salida, superponiendo las pistas superiores sobre las inferiores (picture-in-picture) con posición, escala y opacidad configurables.

## ADDED Requirements

### Requirement: Orden de composición de pistas
El sistema MUST componer las pistas de video de abajo hacia arriba: la pista V1 es el fondo y cada pista de número mayor se dibuja encima. Donde ninguna pista tiene clip, el fondo es negro.

#### Scenario: Dos pistas superpuestas
- **WHEN** V1 y V2 tienen clips en el mismo instante
- **THEN** el clip de V2 se ve encima del de V1

#### Scenario: Solo pista superior
- **WHEN** en un instante solo V2 tiene clip
- **THEN** el clip de V2 se ve con su transformación sobre fondo negro

### Requirement: Transformación de clips superpuestos
Cada clip de una pista superior a V1 MUST tener posición (x, y relativas al lienzo, de 0.0 a 1.0, esquina superior izquierda del clip), escala (de 0.05 a 1.0, como fracción del ancho del lienzo y conservando la relación de aspecto) y opacidad (de 0.0 a 1.0). Por defecto: x=0.65, y=0.65, escala 0.3 y opacidad 1.0.

#### Scenario: PiP por defecto
- **WHEN** el usuario coloca un clip en V2
- **THEN** se ve en la esquina inferior derecha ocupando el 30 % del ancho del lienzo, totalmente opaco

#### Scenario: Cambiar opacidad
- **WHEN** el usuario fija opacidad 0.5 al clip de V2
- **THEN** la imagen de V1 se ve a través del clip de V2 al 50 %

#### Scenario: Cambiar posición y escala
- **WHEN** el usuario fija x=0, y=0 y escala 0.5
- **THEN** el clip ocupa la mitad del ancho del lienzo, anclado en la esquina superior izquierda

### Requirement: Recorte al lienzo
Las partes de un clip superpuesto que quedan fuera del lienzo MUST recortarse y no producir errores.

#### Scenario: Clip parcialmente fuera
- **WHEN** el usuario fija x=0.9 y escala 0.5
- **THEN** solo se ve la parte del clip que cae dentro del lienzo

### Requirement: Efectos y transiciones en pistas superiores
Los efectos y las transiciones de un clip MUST aplicarse antes de la superposición, de modo que se vean dentro del recuadro del PiP.

#### Scenario: Efecto en el PiP
- **WHEN** el clip de V2 tiene escala de grises
- **THEN** el recuadro del PiP se ve en gris y el fondo de V1 conserva sus colores
