#!/usr/bin/env python3
"""Generate warm + golden-ratio wavetables for deiosc.

Warm: tanh(k·sin(2πx)) — a sine passed through gentle saturation, adding
      odd harmonics (3rd, 5th) that decay fast. Analog tube/transistor
      warmth: rich but soft, no inharmonics, no metallic edge.

      WARM_K is the saturation "drive", NOT a volume: the table is
      re-normalized to peak 1.0, so K only shapes the spectrum.
      - K → 0    : tanh(K·sin) ≈ K·sin → pure sine (fundamental only).
      - K = 1.4  : gentle tube saturation — strong 3rd/5th, higher odds
                   decaying quickly. The default deiosc sound.
      - K → large: tanh saturates to ±1 → near-square wave, dense and
                   aggressive odd harmonics.
      WARM_SAT toggles saturation: False → pure sine table (a clean,
      uncolored base waveform for the warm voice).

Gold: 4 partials at phi^(i-1), 1/n^1.5 rolloff, fundamental x2
      (few partials + steep rolloff + 0.6 peak gain => soft, low-volume golden)

Both 256 samples Q15, fitting the minilogue xd per-slot limit.
"""

import math

SIZE = 256
WARM_K = 1.1         # saturation drive: 0 = sine, 1.4 = soft tube, large = square
WARM_SAT = True        # True = tanh saturation, False = pure sine warm table
N_PARTIALS = 4
ROLLOFF = 1.5
GOLD_GAIN = 1
PHI = (1.0 + math.sqrt(5.0)) / 2.0
OUT = "dei_tables.h"

def build_warm():
    if WARM_SAT:
        # Waveshaping tanh(K·sin): K sets the amount of soft-clipping, so it
        # controls the odd-harmonic content (see WARM_K in the header).
        vals = [math.tanh(WARM_K * math.sin(2.0 * math.pi * k / SIZE)) for k in range(SIZE)]
    else:
        # No saturation: the warm table is a pure sine (fundamental only).
        vals = [math.sin(2.0 * math.pi * k / SIZE) for k in range(SIZE)]
    peak = max(abs(v) for v in vals) or 1.0
    return [v / peak for v in vals]

def build_gold():
    amps = [2.0] + [1.0 / ((i + 1) ** ROLLOFF) for i in range(1, N_PARTIALS)]
    ratios = [PHI ** i for i in range(N_PARTIALS)]
    vals = []
    for k in range(SIZE):
        ph = k / SIZE
        v = 0.0
        for amp, r in zip(amps, ratios):
            p = (ph * r) % 1.0
            v += amp * math.sin(2.0 * math.pi * p)
        vals.append(v)
    peak = max(abs(v) for v in vals)
    # Normalize to peak 1.0, then scale down: golden sits lower than the sine.
    return [v / peak * GOLD_GAIN for v in vals]

def to_q15(v):
    q = int(round(v * 32767.0))
    return max(-32767, min(32767, q))

def emit(f, name, vals):
    f.write(f"static const int16_t {name}[{SIZE}] = {{\n")
    for i in range(0, SIZE, 16):
        row = ", ".join(str(to_q15(v)) for v in vals[i:i + 16])
        f.write("  " + row + ("," if i + 16 < SIZE else "") + "\n")
    f.write("};\n\n")

def main():
    with open(OUT, "w") as f:
        f.write("#pragma once\n")
        f.write("// Auto-generated — warm + golden wavetables for deiosc.\n")
        f.write("// SIZE=%d WARM_K=%.4g N_PARTIALS=%d PHI=%.12g Q15\n\n" %
                (SIZE, WARM_K, N_PARTIALS, PHI))
        f.write("#include <stdint.h>\n\n")
        f.write("#define kDeiOscTableSize  %d\n" % SIZE)
        f.write("#define kDeiOscTableMask  (kDeiOscTableSize - 1)\n\n")
        emit(f, "kDeiOscWarm", build_warm())
        emit(f, "kDeiOscGold", build_gold())
    print(f"Wrote {OUT}: warm+gold {SIZE}x2 int16 ({SIZE*4} bytes flash)")

if __name__ == "__main__":
    main()
