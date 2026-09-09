# deiosc

Oscilador digital de **pad ambiental continuo** para KORG minilogue xd (logue-SDK).

**deiosc** no genera notas discretas: es una **textura viva** que evoluciona mientras se mantiene la tecla. Un único modulador interno libre (wander) mantiene el timbre en movimiento perpetuo, sin modulación externa ni parámetros de LFO — el oscilador es 100 % autónomo y determinista.

## Características

- **Dos voces a la misma frecuencia** (`w0`) que leen dos tablas (warm = base, golden) cruzadas por un morph común.
- **Batido por phase rotation** (no detune): un offset de fase rotatorio `s_beat` que rota solo la lectura warm de la voz B → período exacto `1/beatHz`, independiente de la nota y sin multiplicación por armónicos. El golden no bate.
- **Morph warm ↔ golden** con parciales en potencias de φ (1.618ⁿ): a medio camino entre la serie armónica y la campana.
- **Wander autónomo permanente**: LFO interno auto-modulante (~0.0233 Hz, ±50%) que mueve la presencia del golden y el balance de voces.
- **Reloj por tiempo real**: la presencia avanza midiendo el tiempo real con `DWT->CYCCNT` — estable con acordes, release y retrigger; el tiempo inactivo también mueve la presencia.
- **Ataque limpio por nota**: las fases del oscilador audible se resetean a 0 en note-on; la presencia del golden no.
- **Sub octava** mezclable (EDIT 1).

## Controles

| Control físico | Parámetro SDK | Rol |
|---|---|---|
| **SHAPE** (MULTI ENGINE) | `k_user_osc_param_shape` | Escala el morph cálido → golden (0 = sin golden, 1 = golden dominante) |
| **SHIFT + SHAPE** | `k_user_osc_param_shiftshape` | Período del batido (0 = sin batido / señal base pura, 100% = ~1.3 s sin golden) |
| **EDIT 1** | `k_user_osc_param_id1` | **Sub** (0–100%): mezcla de sub octava |

## Uso

Para usar sin compilar, usar /builds/deiosc.mnlgxdunit (válido para Minilogue xd)

## Compilar

Fuera del árbol del SDK, indica la ruta de la plataforma:

```
make PLATFORMDIR=/ruta/logue-sdk/platform/minilogue-xd
```

El binario queda en `build/deiosc.mnlgxdunit`.

## Subir al minilogue xd

Con `logue-cli` (puertos SOUND detectados: in 2 / out 2, slot 4):

```
logue-cli load -u deiosc.mnlgxdunit -i 2 -o 2 -s 4
```

O directamente:

```
SDK_PLATFORM=/ruta/logue-sdk/platform/minilogue-xd ./compile_and_upload.sh
```

## Estructura del proyecto

| Archivo | Rol |
|---|---|
| `deiosc.cpp` | Implementación del oscilador (`OSC_CYCLE`, `OSC_PARAM`, …) |
| `deiosc.hpp` | Clase `DeiOsc`: `Params` (controles) y `State` (estado por voz) |
| `dei_tables.h` | Tablas Q15 precomputadas (warm + golden), 256×2 int16 |
| `gen_tables.py` / `basic_forms.py` | Generadores offline de las tablas |
| `manifest.json` | Metadatos del unit (nombre, parámetros) |
| `project.mk` | Configuración del proyecto (nombre, fuentes) |
| `Makefile` | Build system del logue-SDK |
| `tpl/`, `ld/` | Template de entrada y linker script del SDK |
| `ESPECIFICACION.md` | Especificación técnica del oscilador |
| `PROCESO.md` | Bitácora del proceso de diseño |
| `BLOG_ENTRY_ES.md` | Borrador del artículo técnico |

## Notas de diseño

- El batido se genera con un offset de fase rotatorio **aplicado solo a la lectura warm** de la voz B; el golden se lee en fase (`φ_a ≡ φ_b`) y queda en modo común → no bate.
- **Reloj de presencia por tiempo real**: la fase del golden (`s1`, global de la unidad) se integra midiendo el tiempo real con `DWT->CYCCNT` (84 MHz), no contando voces — estable en polifonía, release y retrigger. El tiempo inactivo también mueve la presencia.
- **Ataque determinista por nota**: `phi_a/b/sub` se resetean a 0 en note-on (cruce por cero); la presencia `s1` no se resetea.
- No hay LFO externo ni Breath: el oscilador es 100 % autónomo.
- No hay ruido ni aleatoriedad: todo es determinista.
- Los formatos de tabla y el balance se documentan en la especificación técnica del proyecto.

---

Proyecto de **DEI Engineering** — https://dei-engineering.tech
