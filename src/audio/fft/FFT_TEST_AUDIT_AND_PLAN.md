# FFT Test Audit and Verification Plan

This is a code-and-test audit for the FFT module.

Scope reviewed:

- `src/audio/fft/fft.c`
- `src/audio/fft/fft.h`
- `tests/test_fft.c`
- `tests/test_fft_verification.c`

## Current coverage (what is already good)

Current tests already validate a useful baseline:

- zero input behavior (`test_zero_input`)
- impulse behavior (`test_impulse`)
- coherent single-tone bin localization for sine/cosine (`test_pure_sine_bin5`, `test_pure_cosine_bin10`)
- broad frequency sweep verification with peak-location, amplitude sanity, and energy concentration (`test_fft_log_sweep_100`)

This is a strong start for correctness of core butterfly math.

## High-risk gaps and inconsistencies found

## 1) Output size contract mismatch (important)

In `fft.c`:

- `fft_compute_raw()` writes bins `0..N/2` (that is `N/2 + 1` values).

In tests:

- `tests/test_fft.c` declares `mag_buf[N/2]`.
- `tests/test_fft_verification.c` allocates `malloc(sizeof(float) * (NFFT/2))`.

So tests are currently providing a buffer that is one float too small for functions that write `N/2 + 1` bins.

Why this matters:

- possible out-of-bounds write,
- may pass by luck in normal runs,
- can hide real defects.

## 2) Header docs and implementation are out of sync

In `fft.h`, comment says `2.0/N normalization`.
In `fft_compute()`, scaling uses approximately `4.0/N` (single-sided + Hann gain compensation).

This can cause tests to assert the wrong expected amplitude if someone relies on header comments.

## 3) Nyquist-bin handling is not explicitly tested

`fft_compute_raw()` emits up to `N/2` inclusive, but `fft_compute()` scaling loop currently goes to `< N/2`.
That means Nyquist bin behavior differs from other bins and is not currently verified by tests.

## 4) API/state behavior is under-tested

No direct tests for:

- invalid `fft_init()` sizes,
- repeat init returns `FFT_ERROR_ALREADY_INIT`,
- `fft_get_size()` before init / after shutdown,
- repeated init/shutdown cycles.

## 5) Numerical robustness corners are thin

Missing targeted checks for:

- DC-only signal (`time_data[i] = constant`),
- Nyquist-only signal (`(-1)^n`),
- leakage profile for off-bin tones,
- behavior across multiple FFT sizes (128..8192).

## Recommended test additions (priority order)

## Priority A (do first)

1. **Fix test output buffer sizes** to `N/2 + 1` everywhere.
2. Add explicit assertions for bins `0` and `N/2` in both raw and windowed paths.
3. Add API contract tests:
   - `fft_init(100)` -> `FFT_ERROR_BAD_SIZE`
   - `fft_init(1000)` (non-power-of-two) -> `FFT_ERROR_BAD_SIZE`
   - `fft_init(valid)` twice -> second is `FFT_ERROR_ALREADY_INIT`
   - `fft_get_size()` transitions around init/shutdown.

## Priority B (strong correctness confidence)

4. **DC test**:
   - input all ones,
   - raw: dominant at bin 0, others near zero.
5. **Nyquist test**:
   - input `x[n] = (-1)^n`,
   - dominant at bin `N/2`.
6. **Off-bin tone leakage test**:
   - compare raw vs Hann-windowed leakage shape,
   - ensure window reduces far-out sidelobes.

## Priority C (quality and regression safety)

7. Run core functional tests at multiple FFT sizes: `128, 256, 1024, 4096, 8192`.
8. Add long-run stability test (many repeated compute calls) to catch state corruption.
9. Add sanitizer-based run (ASan/UBSan) in a debug test target.

## Verification criteria: what "good data" should mean

Use measurable criteria, not only visual checks:

- **Frequency localization**: coherent tones peak at expected bin (±1 with windowed path).
- **Amplitude sanity**: full-scale coherent sine around expected normalized magnitude.
- **Leakage behavior**: off-bin tones produce expected spread; windowed version has reduced sidelobes.
- **Energy concentration**: most energy near target bins for pure tones.
- **Stability**: repeated runs are deterministic within tolerance.
- **Memory safety**: no out-of-bounds writes under sanitizer.

## Suggested immediate next step

Before adding new FFT features, do this sequence:

1. Correct test buffer sizes to `N/2 + 1`.
2. Add API/state tests.
3. Add DC/Nyquist targeted tests.
4. Run both existing suites plus new tests under normal build and sanitizer build.

That gives you high confidence the FFT returns valid, stable, and interpretable data.
