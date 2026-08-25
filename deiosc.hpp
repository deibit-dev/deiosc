#pragma once

#include "logue-osc-common.hpp"

// ============================================================================
// deiosc — oscilador de pad ambiental para KORG minilogue xd
// ----------------------------------------------------------------------------
// Dos voces a la misma frecuencia (w0) leen dos tablas (warm = base, golden)
// y las cruzan con un morph común. El batido sale de un offset de fase
// rotatorio aplicado SOLO a la lectura warm de la voz B; el golden no bate.
// El movimiento tímbrico perpetuo lo aporta un único modulador interno libre
// (s1): no hay LFO externo ni parámetro Breath.
// ============================================================================

struct DeiOsc {

  // ------------------------------------------------------------------------
  // Params — valores de los controles, actualizados por OSC_PARAM().
  // Son los "objetivos" a los que converge el estado audible. No se
  // interpola por muestra: los cambios llegan a nivel de bloque.
  // ------------------------------------------------------------------------
  struct Params {
    float submix;      // EDIT 1 (0..1) — nivel de la sub-octava en la mezcla
                       //   final. 0 = sin sub; 1 = mezcla 50/50 normalizada.
    float shape;       // SHAPE (0..1) — escala el morph warm→golden.
                       //   0 = warm puro (sin golden); 1 = golden dominante.
    float shiftshape;  // SHIFT+SHAPE (0..1) — controla el período del batido.
                       //   0 = sin batido (señal base pura); 1 = batido rápido.

    Params()
      : submix(0.3f),
        shape(0.5f), shiftshape(0.3f) {}
  };

  // ------------------------------------------------------------------------
  // State — estado persistente entre bloques de audio (por instancia/voz).
  // ------------------------------------------------------------------------
  struct State {
    float   phi_a;      // fase de la voz A: lee warm+golden en fase (referencia,
                        //   sin offset de batido).
    float   phi_b;      // fase de la voz B: avanza idéntico a phi_a; sobre ella
                        //   se suma s_beat SOLO en la lectura warm.
    float   phi_sub;    // fase del sub: avanza a w0/2 → una octava abajo.
    float   s1;         // fase del modulador interno libre (wander ~0.0233 Hz).
                        //   Free-running: no se resetea en note-on.
    float   s_beat;     // offset de fase rotatorio del batido (0..1). Avanza a
                        //   beatHz por segundo; 0 = voces en fase (sin batido).
    uint8_t flags;      // banderas del SDK (kOscFlagReset se activa en note-on).

    State()
      : phi_a(0.f), phi_b(0.f), phi_sub(0.f),
        s1(0.75f),                  // seno = -1 → el golden arranca ausente
        s_beat(0.f),                // voces en fase → sin batido al encender
        flags(kOscFlagNone) {}
  };

  Params params;   // controles actuales (ver Params)
  State  state;    // estado persistente del oscilador (ver State)
  float  lpz;      // memoria del filtro one-pole pasa-bajos de salida

  DeiOsc() { init(); }

  void init() {
    state  = State();
    params = Params();
    lpz    = 0.f;
  }
};
