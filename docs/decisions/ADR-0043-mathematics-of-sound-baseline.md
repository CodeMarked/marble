# ADR-0043: Mathematics of sound baseline (Chapter 14 §14.2)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 043** in **§14.2 The Mathematics of Sound**. Building on continuous-wave quantities ([`ADR-0042`](ADR-0042-physics-of-sound-baseline.md)), digital audio needs **sample-rate limits**, **sinusoid phase stepping**, **time–sample indexing**, **linear interpolation** between stored samples, and **PCM quantization**—all without a playback device or FFT library.

## Decision

1. Add [`audio/MathematicsOfSound.hpp`](../../engine/audio/MathematicsOfSound.hpp) under `marble::audio`:
   - `kTwoPi`; `nyquistFrequencyHz`; `angularFrequencyRadiansPerSecond`; `advancePhaseRadians`; `wrapPhaseTwoPi`; `monoSineSample`.
   - `durationSecondsForSampleCount`; `sampleCountCeilForDuration` (ceil in double domain to reduce float drift).
   - `linearInterpolateSamples` for fractional read positions.
   - `floatSampleToInt16` / `int16ToFloatSample` (symmetric ±32768 scale, clamped).
2. Verify with `mathematics_of_sound_test`.

## Consequences

- Positive: synthesis and tooling can advance phases and size buffers deterministically at a chosen sample rate.
- Positive: int16 conversion is a documented first step toward file I/O and hardware feeds.
- Trade-off: no FFT, filters, or resampling kernels yet.
- Follow-up: chunk 044+ for technology/device topics; DSP blocks as dedicated headers when needed.

## Alternatives considered

- **Single header merging §14.1 and §14.2:** rejected; keeps physics vs discrete-time math separable for readers tracing ADRs to chunks.
- **Only Nyquist + no PCM:** rejected; books typically pair sampling with basic quantization in the same chapter slice.
