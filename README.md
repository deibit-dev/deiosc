# DEIOSC

deiosc es un oscilador de usuario para el KORG Minilogue xd. Está orientado a la creación de pads: texturas sostenidas que evolucionan mientras se mantiene una nota. No genera notas monotonas ni secuencias; produce una señal continua cuyo contenido tímbrico cambia de forma autónoma.
## Composición del sonido

El oscilador combina dos componentes:

    * Base senoidal con saturación: proporciona un timbre suave y estable.
    * Componente inarmónico: construido a partir de parciales en potencias del número áureo (φ). Aporta un carácter más brillante e introduce una leve tensión.

La mezcla entre ambos varía automáticamente a lo largo de un ciclo de aproximadamente 51 segundos. Esta modulación es interna y no utiliza el LFO del sintetizador. El usuario puede ajustar la presencia global del componente inarmónico, pero no la velocidad ni la forma de la modulación, que están fijadas por diseño.
Batido

El oscilador incluye un batido cuya intensidad depende de la presencia del componente inarmónico. Cuando este componente disminuye, el batido se hace más audible; cuando aumenta, el batido se atenúa. El período del batido es fijo y no depende de la nota tocada.
Suboscilador

Opcionalmente, se puede añadir un suboscilador para reforzar las frecuencias graves.
## Controles

    * SHAPE: controla la presencia global del componente inarmónico. 0 = solo base; 1 = máximo.
    * SHIFT + SHAPE: controla la intensidad del batido. 0 = sin batido; 1 = máximo.
    * EDIT 1: controla el nivel del suboscilador.

Estos controles ajustan la presencia global de los componentes, pero no alteran la modulación interna automática.
## Uso

Cargue el archivo de unidad en un slot de usuario del Multi Engine del minilogue xd. Seleccione el oscilador y ajuste los controles según el resultado deseado. El LFO del sintetizador queda disponible para otras funciones.

## Notas de diseño

- El batido se genera con un offset de fase rotatorio **aplicado solo a la lectura warm** de la voz B; el golden se lee en fase (`φ_a ≡ φ_b`) y queda en modo común → no bate.
- **Reloj de presencia por tiempo real**: la fase del golden (`s1`, global de la unidad) se integra midiendo el tiempo real con `DWT->CYCCNT` (84 MHz), no contando voces — estable en polifonía, release y retrigger. El tiempo inactivo también mueve la presencia.
- **Ataque determinista por nota**: `phi_a/b/sub` se resetean a 0 en note-on (cruce por cero); la presencia `s1` no se resetea.
- No hay LFO externo ni Breath: el oscilador es 100 % autónomo.
- No hay ruido ni aleatoriedad: todo es determinista.
- Los formatos de tabla y el balance se documentan en la especificación técnica del proyecto.

  Para el visitante interesado en profundizar en como fue el proceso de construcción, creé una publicación con detalles de diseño, algunos detalles de implementación y del proceso de creación: [Construyendo un oscilador personalizado para KORG Minilogue xd](https://dei-bit.tech/2026/08/25/construyendo-un-oscilador-personalizado-para-korg-minilogue-xd/).
---

Proyecto de **DEI-BIT** — https://dei-bit.tech
