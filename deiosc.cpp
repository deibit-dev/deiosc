#include "userosc.h"
#include "deiosc.hpp"
#include "dei_tables.h"

// ---------------------------------------------------------------------------
// Estado GLOBAL de la unidad (compartido entre voces).
// ---------------------------------------------------------------------------
// El reloj de presencia del golden debe ser ÚNICO para toda la unidad: todas
// las voces deben leer el mismo w1 (mismo morph) y el reloj debe correr a
// tiempo real. Se mide el tiempo real con el contador de ciclos del Cortex-M4
// (DWT->CYCCNT): entre llamadas consecutivas a OSC_CYCLE se integra el dt de
// ciclos (convertido a segundos con la frecuencia de CPU) acumulando la fase
// en g_s1. Así no se depende del nº de voces ni de NOTEON/NOTEOFF: el reloj
// corre a tiempo real haya 1 o N voces, release, stealing, etc.
// DeiOsc (State/phi/lpz) queda reservado al estado por voz.

// Fase del modulador interno de presencia (wander ~0.0233 Hz), GLOBAL: una sola
// por unidad, compartida por todas las voces. Arranca en 0.75 (seno = -1 →
// golden ausente). Free-running: nunca se resetea en note-on.
static float g_s1 = 0.75f;

// --- Reloj de ciclos (DWT) --------------------------------------------------
// STM32F401 a 84 MHz. DWT->CYCCNT es un contador de 32 bits que se incrementa
// por ciclo de CPU; se lee por diferencia entre llamadas (wrap natural en
// unsigned). Se convierte a segundos y se acumula en g_s1 (fase, en float),
// de modo que el overflow del contador de 32 bits no importa: nunca se usa
// como valor absoluto, solo como dt entre llamadas consecutivas.
#define DEIOSC_DEMCR      (*(volatile uint32_t *)0xE000EDFCu)  // CoreDebug DEMCR
#define DEIOSC_DWT_CTRL   (*(volatile uint32_t *)0xE0001000u)  // DWT->CTRL
#define DEIOSC_DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)  // DWT->CYCCNT

static const float kCpuHz = 84000000.f;   // frecuencia de CPU (84 MHz)

static uint32_t g_prev_cyc  = 0u;
static uint8_t  g_cyc_valid = 0u;         // primera llamada: sin dt previo

// Tasa base del modulador interno de presencia (wander, ~0.0233 Hz) por
// segundo, auto-modulada: rate_hz = kW0S1Hz * (1 + 0.5*w1).
static const float kW0S1Hz = 0.0233f;

// Instancia única del oscilador (una por llamada a OSC_CYCLE).
static DeiOsc s_osc;

// ---------------------------------------------------------------------------
// Reloj de presencia (DWT)
// ---------------------------------------------------------------------------

// Habilita el contador de ciclos del Cortex-M4 y deja el reloj listo para
// integrar. Se llama desde OSC_INIT.
static void presence_init(void)
{
  // DEMCR.TRCENA + DWT.CYCCNTENA habilitan el contador de ciclos.
  DEIOSC_DEMCR      |= (1u << 24);
  DEIOSC_DWT_CYCCNT  = 0u;
  DEIOSC_DWT_CTRL   |= (1u << 0);
  g_prev_cyc  = 0u;
  g_cyc_valid = 0u;
}

// Avanza la fase g_s1 por el tiempo REAL transcurrido desde la llamada anterior
// a OSC_CYCLE, medido con DWT->CYCCNT. Las llamadas son consecutivas en el
// tiempo real de CPU, así que la suma de sus dt equivale al tiempo que pasa de
// verdad, haya 1 o N voces, release, stealing, etc. El tiempo inactivo (huecos
// entre notas) también avanza la presencia: es un reloj de pared continuo.
// El dt es correcto para cualquier hueco < 2^32 ciclos (~51 s a 84 MHz);
// huecos mayores a un wrap completo no son medibles con un contador de 32 bits.
static void presence_tick(void)
{
  const uint32_t now = DEIOSC_DWT_CYCCNT;
  if (g_cyc_valid) {
    const uint32_t dtc = now - g_prev_cyc;   // wrap natural en unsigned
    if (dtc != 0u) {
      const float dt_s = (float)dtc / kCpuHz;   // ciclos → segundos
      const float w1r  = read_q15(kDeiOscWarm, g_s1, kDeiOscTableSize,
                                  kDeiOscTableMask);
      // rate en Hz = kW0S1Hz·(1 + 0.5·w1) → fase += rate·dt
      g_s1 += kW0S1Hz * (1.f + 0.5f * w1r) * dt_s;
      g_s1 -= (uint32_t)g_s1;                   // mantiene fase en [0,1)
    }
  }
  g_prev_cyc  = now;
  g_cyc_valid = 1u;
}

// Crossfade warm↔golden en fase de tabla: warm + morph·(golden − warm).
static inline float xfade(float warm, float golden, float morph)
{
  return warm + morph * (golden - warm);
}

// ---------------------------------------------------------------------------
// Hooks del SDK
// ---------------------------------------------------------------------------

void OSC_INIT(uint32_t platform, uint32_t api)
{
  (void)platform; (void)api;
  s_osc.init();
  g_s1 = 0.75f;          // golden ausente al encender
  presence_init();
}

void OSC_CYCLE(const user_osc_param_t * const params,
               int32_t *yn, const uint32_t frames)
{
  if (frames == 0u) return;

  DeiOsc::State &st = s_osc.state;
  const DeiOsc::Params &p = s_osc.params;

  presence_tick();

  // Frecuencia angular de la nota (incremento de fase por muestra).
  const float w0 = osc_w0f_for_note((params->pitch)>>8, params->pitch & 0xFF);
  if (w0 <= 0.f) {
    q31_t *y = (q31_t *)yn;
    for (uint32_t i = 0; i < frames; ++i) y[i] = 0;
    return;
  }

  // En note-on el SDK pide reset de fase. Se resetean SOLO las fases de la
  // forma de onda audible de ESTA voz (phi_a/b/sub): cada nota arranca las
  // tablas en fase 0 (cruce por cero → ataque limpio y determinista). El reloj
  // global de presencia g_s1 NO se resetea: es una secuencia continua única
  // compartida por todas las voces.
  if (st.flags & kOscFlagReset) {
    st.flags &= ~kOscFlagReset;
    st.phi_a   = 0.f;
    st.phi_b   = 0.f;
    st.phi_sub = 0.f;
  }

  // Copia local del estado: se trabaja sobre registros a lo largo del bucle.
  float phi_a   = st.phi_a;
  float phi_b   = st.phi_b;
  float phi_sub = st.phi_sub;
  float s1      = g_s1;          // reloj GLOBAL de presencia (compartido)
  float s_beat  = st.s_beat;

  // --- Modulación lenta (a nivel de bloque) ---------------------------------
  // w1 = salida del modulador interno (seno leído de la tabla warm, sin
  // matemática extra). Maneja la presencia del golden y modula su propia
  // tasa → LFO auto-modulante cuyo período nunca se asienta. Es la ÚNICA
  // fuente de modulación: no hay LFO externo.
  const float w1 = read_q15(kDeiOscWarm, s1, kDeiOscTableSize, kDeiOscTableMask);

  const float submix = p.submix;
  const float invmix = 1.f / (1.f + submix);  // normaliza la mezcla con el sub
  float lpz = s_osc.lpz;

  // --- Morph (crossfade warm↔golden) ---------------------------------------
  // morph = SHAPE × (1 + morph_w)
  //   · SHAPE = 0 → morph = 0 → el golden nunca aparece (warm puro).
  //   · El wander (morph_w) está siempre a fuerza completa: es el "corazón
  //     continuo" que hace respirar el golden sin ningún control externo.
  // Constantes dentro del bloque: se calculan una vez, fuera del bucle.
  float morph = p.shape * (1.f + w1) * 0.5f;
  const float mix = 0.80f + 0.10f * morph;
  // --- Beat (frecuencia del batido) -----------------------------------------
  // Rango ~0.62..0.75 Hz según morph. Shift+Shape = 0 → beatHz = 0 exacto
  // (sin batido); el acople golden escala con el knob para que el cero sea
  // exacto.
  const float beatHz = p.shiftshape * ( 0.62f + (1.f - morph) * 0.13f );
  // Incremento de batido por muestra. s_beat es POR VOZ (vive en State): cada
  // voz avanza su propio batido a tiempo real, sin dividir por nº de voces.
  const float beat_inc = beatHz * k_samplerate_recipf;

  q31_t * __restrict y = (q31_t *)yn;
  const q31_t * y_e = y + frames;

  for (; y != y_e; ) {
    // --- Voz A: referencia, sin offset de batido ----------------------------
    // Lee warm y golden en phi_a. phi_a y phi_b son acumuladores que avanzan
    // idénticos (+= w0), por lo que phi_a == phi_b siempre.
    float sig_a = xfade(read_q15(kDeiOscWarm, phi_a, kDeiOscTableSize, kDeiOscTableMask),
                        read_q15(kDeiOscGold, phi_a, kDeiOscTableSize, kDeiOscTableMask),
                        morph);

    // --- Voz B: solo su lectura warm lleva el offset de batido --------------
    // El offset s_beat se suma en el momento de LEER la tabla (no mueve el
    // acumulador de fase phi_b). El golden se lee en fase (phi_b == phi_a),
    // por lo que es común a ambas voces y NO bate: solo latea la base (warm).
    float pb = phi_b + s_beat;
    pb -= (uint32_t)pb;                          // normaliza a [0,1)

    float sig_b = xfade(read_q15(kDeiOscWarm, pb, kDeiOscTableSize, kDeiOscTableMask),
                        read_q15(kDeiOscGold, phi_b, kDeiOscTableSize, kDeiOscTableMask),
                        morph);

    // Mezcla de voces: sig = mix·sig_a + (1−mix)·sig_b. El batido audible es
    // la interferencia entre el warm fijo (voz A) y el warm rotando (voz B).
    float sig = sig_a * mix + sig_b * (1.f - mix);

    // --- Sub octava ---------------------------------------------------------
    // Lee a phi_sub (avanza a w0/2 → una octava abajo) con el mismo morph.
    float sub = xfade(read_q15(kDeiOscWarm, phi_sub, kDeiOscTableSize, kDeiOscTableMask),
                      read_q15(kDeiOscGold, phi_sub, kDeiOscTableSize, kDeiOscTableMask),
                      morph);
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
    s_beat  += beat_inc;
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
  // g_s1 ya se integró en presence_tick() con el dt real (DWT).
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
