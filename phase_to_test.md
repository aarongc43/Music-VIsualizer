---

## Phase 1: Establish a Golden-Path Test Fixture

**Goal:** Create deterministic audio files so we can write true end-to-end tests, not just unit tests on `vis_bars_compute`.

1. **Generate simple WAVs**

   * **Sine tone**: 440 Hz, 1 s, 44 100 Hz, 16-bit PCM mono.
   * **Frequency sweep**: 20 Hz → 20 kHz over 5 s, same format.
2. **Place them in** `tests/fixtures/` (or `src/fixtures/`).
3. **Automate creation** via a short Python script (stdlib `wave` + `math` + `struct`) or an `ffmpeg` one-liner, so it lives in your repo and can be regenerated.

*Outcome:* We know exactly what’s in the signal and where it should map to in the spectrum.

---

## Phase 2: Build an End-to-End Test Harness

**Goal:** Verify the entire pipeline—from file → samples → FFT → dB → smoothing → bar heights—against our fixtures.

1. **New test file** `tests/test_vis_bars_e2e.c`
2. **Load** `sine440.wav` via your existing `wav_load` (in `audio_engine.c`).
3. **Optionally downmix/resample** so you’re exercising those code paths too.
4. **Run FFT** (from `fft.c`) on a single frame of N samples.
5. **Feed magnitudes** through `vis_convert_to_dbfs`, `vis_apply_*` and finally `vis_bars_compute`.
6. **Compute expected bar index** for 440 Hz:

   ```c
   float bin_hz = (float)sample_rate / N;
   size_t bin_idx = (size_t)roundf(440.0f / bin_hz);
   int expected_bar = find_bar_for_bin(bin_idx);
   ```
7. **Assert** only that bar’s height > `BAR_MIN_H`, all others == `BAR_MIN_H`.

*Outcome:* We’ll know whether your grouping, scaling and thresholds are logically correct.

---

## Phase 3: Instrument & Log Intermediate Values

**Goal:** See exactly how your data evolves at each step.

1. In `vis_update()` (in `visualization_engine.c`), after each stage:

   * **Print** or write to a small circular buffer:

     * Raw `magnitudes[k]` for a single bin (e.g. k=0, k=N/4, k=N-1).
     * After dB conversion: `dst[k]`.
     * After temporal & spatial smoothing.
2. **Run** your fixture through the visualizer for a few frames, capture the logs.

*Outcome:* You’ll spot if, for instance, your dB mapping is already pegged to 1.0 before the bars even see it.

---

## Phase 4: Tighten & Refine the Algorithm

**Goal:** Once we know *where* things go wrong, tune the right knobs in code.

1. **Bin‐to‐bar mapping**

   * Confirm linear vs. log grouping matches tests & musical expectations.
2. **Dynamic‐range compression**

   * Verify your `powf` exponent is applied exactly once—and that it compresses, not over-boosts.
3. **dB floor/ceiling**

   * Consider adaptive floor/ceiling per fixture.
4. **Overlapping frames**

   * Implement 50 % overlap + averaging if single-frame data is too spiky.

*Outcome:* The visuals will start to look smooth and musically meaningful.

---

## Phase 5: Update & Extend Unit Tests

**Goal:** Codify all the insights from Phases 2–4 back into your `test_vis_bars.c` and new tests:

1. **Add tests** for non-uniform grouping (e.g. assert bar widths in `g_edges`).
2. **Test** your power-law scaling function in isolation.
3. **Validate** that a very low-level tone sits at zero after thresholding.

*Outcome:* Your CI suite will catch any regressions instantly, and you’ll have both unit and end-to-end coverage.

---
