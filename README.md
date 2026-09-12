# DEIOSC (Español)

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

Descargue el oscilador desde la sección releases. Cargue el archivo de unidad en un slot de usuario del Multi Engine del minilogue xd. Seleccione el oscilador y ajuste los controles según el resultado deseado. El LFO del sintetizador queda disponible para otras funciones.

## Notas de diseño

- El batido se genera con un offset de fase rotatorio **aplicado solo a la lectura warm** de la voz B; el golden se lee en fase (`φ_a ≡ φ_b`) y queda en modo común → no bate.
- **Reloj de presencia por tiempo real**: la fase del golden (`s1`, global de la unidad) se integra midiendo el tiempo real con `DWT->CYCCNT` (84 MHz), no contando voces — estable en polifonía, release y retrigger. El tiempo inactivo también mueve la presencia.
- **Ataque determinista por nota**: `phi_a/b/sub` se resetean a 0 en note-on (cruce por cero); la presencia `s1` no se resetea.
- No hay LFO externo ni Breath: el oscilador es 100 % autónomo.
- No hay ruido ni aleatoriedad: todo es determinista.
- Los formatos de tabla y el balance se documentan en la especificación técnica del proyecto.

  Para el visitante interesado en profundizar en como fue el proceso de construcción, creé una publicación con detalles de diseño, algunos detalles de implementación y del proceso de creación: [Construyendo un oscilador personalizado para KORG Minilogue xd](https://dei-bit.tech/2026/08/25/construyendo-un-oscilador-personalizado-para-korg-minilogue-xd/).

## Demos

[![Demo introductoria al oscilador](https://youtu.be/hEEluVHcMvQ)]

[![Demo con preset que combina deiosc+vco](https://youtu.be/hwX-FXrKZW0)]


---

Proyecto de **DEI-BIT** — https://dei-bit.tech

----------------------------------------------------------

# DEIOSC (English)

deiosc is a user oscillator for the KORG Minilogue xd. It is oriented toward creating pads: sustained textures that evolve while a note is held. It does not generate monotonous notes or sequences; it produces a continuous signal whose timbral content changes autonomously.
## Sound composition

The oscillator combines two components:

    * Sine base with saturation: provides a smooth and stable timbre.
    * Inharmonic component: built from partials at powers of the golden ratio (φ). It adds a brighter character and introduces a slight tension.

The mix between the two varies automatically over a cycle of approximately 51 seconds. This modulation is internal and does not use the synthesizer's LFO. The user can adjust the overall presence of the inharmonic component, but not the speed or shape of the modulation, which are fixed by design.
Beating

The oscillator includes beating whose intensity depends on the presence of the inharmonic component. When this component decreases, the beating becomes more audible; when it increases, the beating is attenuated. The beating period is fixed and does not depend on the note played.
Sub-oscillator

Optionally, a sub-oscillator can be added to reinforce low frequencies.
## Controls

    * SHAPE: controls the overall presence of the inharmonic component. 0 = base only; 1 = maximum.
    * SHIFT + SHAPE: controls the beating intensity. 0 = no beating; 1 = maximum.
    * EDIT 1: controls the sub-oscillator level.

These controls adjust the overall presence of the components, but do not alter the internal automatic modulation.
## Usage

Download the oscillator from the releases section. Load the unit file into a user slot of the minilogue xd's Multi Engine. Select the oscillator and adjust the controls according to the desired result. The synthesizer's LFO remains available for other functions.

## Design notes

- Beating is generated with a rotating phase offset **applied only to the warm reading** of voice B; the golden is read in phase (`φ_a ≡ φ_b`) and remains in common mode → it does not beat.
- **Real-time presence clock**: the phase of the golden (`s1`, global to the unit) is integrated by measuring real time with `DWT->CYCCNT` (84 MHz), not by counting voices — stable under polyphony, release, and retrigger. Idle time also moves the presence.
- **Deterministic per-note attack**: `phi_a/b/sub` are reset to 0 on note-on (zero crossing); the `s1` presence is not reset.
- There is no external LFO or Breath: the oscillator is 100% autonomous.
- There is no noise or randomness: everything is deterministic.
- Table formats and balance are documented in the project's technical specification.

  For visitors interested in going deeper into how the construction process went, I created a post with design details, some implementation details, and the creation process: [Building a custom oscillator for KORG Minilogue xd](https://dei-bit.tech/2026/08/25/construyendo-un-oscilador-personalizado-para-korg-minilogue-xd/).

## Demos

[![Introductory demo of the oscillator](https://youtu.be/hEEluVHcMvQ)]

[![Demo with a preset combining deiosc+vco](https://youtu.be/hwX-FXrKZW0)]



---

Project by **DEI-BIT** — https://dei-bit.tech
