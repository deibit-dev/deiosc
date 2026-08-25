#include "userosc.h"
#include "deiosc.hpp"
#include "dei_tables.h"

// Instancia única del oscilador (una por llamada a OSC_CYCLE).
static DeiOsc s_osc;

// ---------------------------------------------------------------------------
// Constantes del batido (beat)
// ---------------------------------------------------------------------------
// El batido se genera con un offset de fase rotatorio en la lectura de la
// tabla warm de la voz B (NO por desafinado de frecuencia), de modo que el
// período (1/beatHz) es independiente de la nota y sin multiplicación por
// armónicos. Shift+Shape mapea el rango completo:
//   0   → 0 Hz (sin batido: las voces quedan en fase, señal base pura)
//   100% → ~0.75 Hz (~1.3 s) sin golden.
static const float kBeatHzBase   = 0.12f;  // extremo bajo del knob (parte del rango 0..0.62 Hz)
static const float kBeatHzShift  = 0.50f;  // Shift+Shape → 0..0.62 Hz (dominante)
static const float kBeatHzSpread = 0.13f;  // sin golden → +0.13 Hz (acople secundario)

// Tasa base del modulador interno de presencia (wander, ~0.0233 Hz),
// expresada como incremento de fase por muestra.
static const float kW0S1 = 0.0233f * k_samplerate_recipf;

void OSC_INIT(uint32_t platform, uint32_t api)
{
  (void)platform; (void)api;
  s_osc.init();
}

void OSC_CYCLE(const user_osc_param_t * const params,
               int32_t *yn, const uint32_t frames)
{
  if (frames == 0u) return;

  DeiOsc::State &st = s_osc.state;
  const DeiOsc::Params &p = s_osc.params;

  // Frecuencia angular de la nota (incremento de fase por muestra).
  const float w0 = osc_w0f_for_note((params->pitch)>>8, params->pitch & 0xFF);
  if (w0 <= 0.f) {
    q31_t *y = (q31_t *)yn;
    for (uint32_t i = 0; i < frames; ++i) y[i] = 0;
    return;
  }

  // En note-on el SDK pide reset de fase. Aquí NO se resetean phi_a/b/sub ni
  // s1: deiosc es free-running, la secuencia sonora es continua y el golden
  // no se reinicia (solo se limpia la bandera).
  if (st.flags & kOscFlagReset) {
    st.flags &= ~kOscFlagReset;
    // (fases de oscilador y s1 se dejan correr a propósito)
  }

  // Copia local del estado: se trabaja sobre registros a lo largo del bucle.
  float phi_a   = st.phi_a;
  float phi_b   = st.phi_b;
  float phi_sub = st.phi_sub;
  float s1      = st.s1;
  float s_beat  = st.s_beat;

  // --- Modulación lenta (a nivel de bloque) ---------------------------------
  // w1 = salida del modulador interno (seno leído de la tabla warm, sin
  // matemática extra). Maneja la presencia del golden y modula su propia
  // tasa → LFO auto-modulante cuyo período nunca se asienta. Es la ÚNICA
  // fuente de modulación: no hay LFO externo.
  const float w1 = read_q15(kDeiOscWarm, s1, kDeiOscTableSize, kDeiOscTableMask);
  // Balance rotatorio entre voces: la voz A domina (0.70..0.90) para que la
  // cancelación en antifase del batido quede suave y el sonido solo se meza.
  // Tasa del wander auto-modulada ±50% según su propio valor.
  const float rate1 = kW0S1 * (1.f + 0.5f * w1);

  const float submix = p.submix;
  const float invmix = 1.f / (1.f + submix);  // normaliza la mezcla con el sub
  float lpz = s_osc.lpz;

  q31_t * __restrict y = (q31_t *)yn;
  const q31_t * y_e = y + frames;

  for (; y != y_e; ) {
    // --- Morph (crossfade warm↔golden) -------------------------------------
    // morph = SHAPE × (1 + morph_w)
    //   · SHAPE = 0 → morph = 0 → el golden nunca aparece (warm puro).
    //   · El wander (morph_w) está siempre a fuerza completa: es el "corazón
    //     continuo" que hace respirar el golden sin ningún control externo.
    float morph = p.shape * (1.f + w1) * 0.5f;
    const float mix = 0.80f + 0.10f * morph;
    // --- Beat (frecuencia del batido) ---------------------------------------
    // Rango 0..~0.75 Hz. Shift+Shape = 0 → beatHz = 0 exacto (sin batido);
    // el acople golden escala con el knob para que el cero sea exacto.
    const float beatHz = p.shiftshape * ( 0.62f + (1.f - morph) * 0.13f );

    // --- Voz A: referencia, sin offset de batido ----------------------------
    // Lee warm y golden en phi_a. phi_a y phi_b son acumuladores que avanzan
    // idénticos (+= w0, sin reset), por lo que phi_a == phi_b siempre.
    float sa = read_q15(kDeiOscWarm, phi_a, kDeiOscTableSize, kDeiOscTableMask);
    float ga = read_q15(kDeiOscGold, phi_a, kDeiOscTableSize, kDeiOscTableMask);
    float sig_a = sa + morph * (ga - sa);

    // --- Voz B: solo su lectura warm lleva el offset de batido --------------
    // El offset s_beat se suma en el momento de LEER la tabla (no mueve el
    // acumulador de fase phi_b). El golden se lee en fase (phi_b == phi_a),
    // por lo que es común a ambas voces y NO bate: solo latea la base (warm).
    float pb = phi_b + s_beat;
    if (pb < 0.f) pb += 1.f;

    float sb = read_q15(kDeiOscWarm, pb, kDeiOscTableSize, kDeiOscTableMask);
    float gb = read_q15(kDeiOscGold, phi_b, kDeiOscTableSize, kDeiOscTableMask);
    float sig_b = sb + morph * (gb - sb);

    // Mezcla de voces: sig = mix·sig_a + (1−mix)·sig_b. El batido audible es
    // la interferencia entre el warm fijo (voz A) y el warm rotando (voz B).
    float sig = sig_a * mix + sig_b * (1.f - mix);

    // --- Sub octava ---------------------------------------------------------
    // Lee a phi_sub (avanza a w0/2 → una octava abajo) con el mismo morph.
    float ss = read_q15(kDeiOscWarm, phi_sub, kDeiOscTableSize, kDeiOscTableMask);
    float gs = read_q15(kDeiOscGold, phi_sub, kDeiOscTableSize, kDeiOscTableMask);
    float sub = ss + morph * (gs - ss);
    sig += submix * sub;
    sig *= invmix;

    // --- Filtro one-pole + ganancia de salida -------------------------------
    sig = lpz + 0.5f * (sig - lpz);   // suaviza el registro agudo
    lpz = sig;
    sig *= 0.85f;
    *(y++) = f32_to_q31(sig);

    // --- Avance de fases ----------------------------------------------------
    phi_a   += w0;      phi_a   -= (uint32_t)phi_a;
    phi_b   += w0;      phi_b   -= (uint32_t)phi_b;
    phi_sub += w0*0.5f; phi_sub -= (uint32_t)phi_sub;
    s_beat  += beatHz * k_samplerate_recipf;
    s_beat  -= (uint32_t)s_beat;
    // Con el knob en 0 (beatHz = 0) s_beat decae hacia 0 para que las voces
    // converjan a unison (señal base pura) sin click.
    if (beatHz == 0.f) s_beat *= 0.9999f;
  }

  // --- Guardar estado para el próximo bloque --------------------------------
  st.phi_a   = phi_a;
  st.phi_b   = phi_b;
  st.phi_sub = phi_sub;
  st.s_beat  = s_beat;
  s1 += rate1 * frames;  s1 -= (uint32_t)s1;
  st.s1     = s1;
  s_osc.lpz = lpz;
}

void OSC_NOTEON(const user_osc_param_t * const params) {
  (void)params;
  s_osc.state.flags |= kOscFlagReset;
}

void OSC_NOTEOFF(const user_osc_param_t * const params) {
  (void)params;
}

// Recepción de cambios de parámetro desde el panel / firmware.
void OSC_PARAM(uint16_t index, uint16_t value) {
  DeiOsc::Params &p = s_osc.params;
  switch (index) {
  case k_user_osc_param_id1:          // EDIT 1 = Sub
    p.submix = (value > 100) ? 1.f : (value * 0.01f); break;
  case k_user_osc_param_shape:        // SHAPE
    p.shape = param_val_to_f32(value); break;
  case k_user_osc_param_shiftshape:   // SHIFT+SHAPE
    p.shiftshape = param_val_to_f32(value); break;
  default: break;
  }
}
