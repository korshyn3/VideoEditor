# timeline-preview Specification

## Purpose

Reproducir el timeline dentro de la aplicación para revisar la edición, con play/pausa, un cursor arrastrable y el resultado final visible (efectos, transiciones, PiP y audio mezclado).

## Requirements

### Requirement: Play y pausa
El usuario MUST poder iniciar y pausar la reproducción del timeline desde la posición del cursor, con un botón o con la barra espaciadora. Al llegar al final del timeline, la reproducción se detiene.

#### Scenario: Reproducir
- **WHEN** el cursor está en 3 s y el usuario pulsa play
- **THEN** el preview muestra el timeline desde 3 s avanzando en tiempo real y el cursor se desplaza

#### Scenario: Fin del timeline
- **WHEN** la reproducción alcanza el final del último clip
- **THEN** la reproducción se pausa y el cursor queda en el final

### Requirement: Cursor arrastrable (seek)
El usuario MUST poder arrastrar el cursor o hacer clic en la regla de tiempo para ir a cualquier instante. El preview muestra el fotograma correspondiente a esa posición, también mientras se arrastra.

#### Scenario: Seek en pausa
- **WHEN** el usuario hace clic en 12.5 s con la reproducción pausada
- **THEN** el preview muestra el fotograma del timeline en 12.5 s

#### Scenario: Seek durante reproducción
- **WHEN** el usuario arrastra el cursor durante la reproducción
- **THEN** la reproducción continúa desde la nueva posición al soltarlo

### Requirement: Preview fiel al resultado
El preview MUST mostrar los efectos, las transiciones, la composición PiP y el audio mezclado de la misma forma que la exportación, aunque puede hacerlo a menor resolución.

#### Scenario: Ver una transición
- **WHEN** la reproducción pasa por una transición de tipo círculo
- **THEN** el preview muestra la animación circular entre los dos clips

### Requirement: Sincronía de audio y video
Durante la reproducción, el audio MUST mantenerse sincronizado con la imagen. Si el renderizado no alcanza tiempo real, el sistema descarta fotogramas de video en lugar de atrasar o detener el audio.

#### Scenario: Timeline pesado
- **WHEN** el timeline tiene dos PiP con desenfoque y el equipo no llega a renderizar en tiempo real
- **THEN** el video avanza a saltos pero sigue sincronizado con el audio

### Requirement: Preview refleja las ediciones
Cualquier cambio en el modelo (cortes, efectos, transiciones, transformaciones o mute) MUST reflejarse en el preview sin reiniciar la aplicación: inmediatamente si está en pausa, o en el siguiente fotograma si está reproduciendo.

#### Scenario: Cambiar un efecto en pausa
- **WHEN** el usuario mueve el control de brillo con la reproducción pausada
- **THEN** el fotograma mostrado se actualiza con el nuevo brillo

### Requirement: Interfaz no bloqueante
La decodificación del preview MUST ejecutarse sin congelar la interfaz: la ventana sigue respondiendo a clics y redibujándose mientras se decodifica.

#### Scenario: Seek a un archivo grande
- **WHEN** el usuario hace seek a una posición de un archivo 4K de larga duración
- **THEN** la interfaz sigue respondiendo mientras se obtiene el fotograma
