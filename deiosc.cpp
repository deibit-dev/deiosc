#include "userosc.h"
#include "deiosc.hpp"
#include "dei_tables.h"

// ---------------------------------------------------------------------------
// Presencia GLOBAL por reloj de ciclos (hardware, sin estado por voz).
// ---------------------------------------------------------------------------
// El firmware de minilogue-xd instancia el unit POR VOZ: cada slot tiene su
// propia copia de las variables estáticas, así que NADA declarado como
// `static` es compartido entre voces. El ÚNICO recurso realmente compartido es
// el hardware: DWT->CYCCNT es un registro físico del Cortex-M4, el mismo para
// todos los slots en un instante dado.
//
// Por eso la fase de presencia NO se acumula (acumular diverge por slot): se
// COMPUTA directamente desde CYCCNT como fase fija-point, alineada con el wrap
// del contador de 32 bits. La fase es pura función del tiempo de hardware,
// idéntica en todas las voces, y continua a través del wrap (el período de
// presencia coincide con 2^32 ciclos ≈ 51 s a 84 MHz). Sin auto-modulación:
// la marea es un seno de fase global, determinista y por construcción idéntico
// entre notas → la continuidad de la presencia ya no depende de qué slot toca.

// Registros del core (STM32F401).
#define DEIOSC_DEMCR      (*(volatile uint32_t *)0xE000EDFCu)  // CoreDebug DEMCR
#define DEIOSC_DWT_CTRL   (*(volatile uint32_t *)0xE0001000u)  // DWT->CTRL
#define DEIOSC_DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)  // DWT->CYCCNT

// Instancia única del oscilador (una por llamada a OSC_CYCLE).
static DeiOsc s_osc;

// Habilita el contador de ciclos del Cortex-M4 (DEMCR.TRCENA + DWT.CYCCNTENA).
static void presence_init(void)
{
  DEIOSC_DEMCR     |= (1u << 24);
  DEIOSC_DWT_CTRL  |= (1u << 0);
  DEIOSC_DWT_CYCCNT = 0u;   // ancla el origen de la fase de presencia
}

// Fase de presencia global en [0,1), derivada del contador de ciclos. Usa los
// 24 bits altos de CYCCNT: resolución de 2^-24 y período de 2^32 ciclos (~51 s
// a 84 MHz). La fase cierra exacto en el wrap, así que nunca salta y es la
// misma para todas las voces. El offset de +0.75 hace que al encender arranque
// en golden ausente (warm = -1), igual que el diseño original.
static inline float presence_phase(void)
{
  const uint32_t c = DEIOSC_DWT_CYCCNT;
  const uint32_t p = ((c >> 8) + 12582912u) & 0x00FFFFFFu;  // +0.75 de vuelta
  return (float)p * (1.f / 16777216.f);                     // 2^-24
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
  presence_init();
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

  // En note-on el SDK pide reset de fase. Las fases de la forma de onda audible
  // se dejan correr (reset comentado): la presencia del golden es global por
  // hardware y la continuidad entre notas ya no depende del slot.
  if (st.flags & kOscFlagReset) {
    st.flags &= ~kOscFlagReset;
  }

  // Copia local del estado: se trabaja sobre registros a lo largo del bucle.
  float phi_a   = st.phi_a;
  float phi_b   = st.phi_b;
  float phi_sub = st.phi_sub;
  float s_beat  = st.s_beat;

  // --- Modulación lenta (a nivel de bloque) ---------------------------------
  // w = lectura de la tabla warm en la fase de presencia GLOBAL, derivada del
  // reloj de ciclos de hardware (misma para todas las voces, continua en el
  // wrap). Señal ∈ [−1, 1].
  const float w = read_q15(kDeiOscWarm, presence_phase(), kDeiOscTableSize,
                           kDeiOscTableMask);

  const float submix = p.submix;
  const float invmix = 1.f / (1.f + submix);  // normaliza la mezcla con el sub
  float lpz = s_osc.lpz;

  // --- Morph (crossfade warm↔golden) ---------------------------------------
  // morph = SHAPE × (1 + morph_w)
  //   · SHAPE = 0 → morph = 0 → el golden nunca aparece (warm puro).
  //   · El wander (morph_w) está siempre a fuerza completa: es el "corazón
  //     continuo" que hace respirar el golden sin ningún control externo.
  // Constantes dentro del bloque: se calculan una vez, fuera del bucle.
  float morph = p.shape * (1.f + w) * 0.5f;
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
  // La presencia se computa desde CYCCNT (hardware): no hay estado que guardar.
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
