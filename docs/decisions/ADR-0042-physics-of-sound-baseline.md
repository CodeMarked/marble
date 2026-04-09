# ADR-0042: Physics of sound baseline (Chapter 14 §14.1)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 042** in **Chapter 14 — Audio** through **§14.1 The Physics of Sound**. Before sample-rate math, DSP, or device APIs (later §§14.2+), the engine benefits from **shared constants and linear formulas**: wavelength vs frequency, decibel amplitude ratios, SPL reference pressure, and simple spherical spreading.

## Decision

1. Add [`audio/PhysicsOfSound.hpp`](../../engine/audio/PhysicsOfSound.hpp) under `marble::audio`:
   - Constants: `kSpeedOfSoundDryAir20C` (343 m/s), `kReferenceSoundPressurePa` (20 μPa).
   - `wavelengthMeters` / `frequencyFromWavelengthHz` / `periodSeconds`.
   - Amplitude-style dB: `amplitudeRatioToDecibels` / `decibelsToAmplitudeRatio` (20 log10).
   - SPL: `soundPressureLevelDecibels` / `splToSoundPressurePascals`.
   - Free-field point source pressure scale `pointSourcePressureScale` (1/r vs reference distance).
2. Verify with `physics_of_sound_test`.

## Consequences

- Positive: gameplay and tools can convert between physical and dB units without pulling an audio SDK.
- Positive: spreading helper gives a documented first step toward distance attenuation curves.
- Trade-off: no temperature/humidity model, no obstruction or diffusion.
- Follow-up: chunk 043+ for sampling/FFT math; later chunks for buffers, voices, and hardware.

## Alternatives considered

- **Defer all audio until a middleware SDK:** rejected; §14.1 is explicitly physics-first and stays dependency-free.
- **10 log10 power rule only:** rejected; game gain lines use 20 log10 for amplitudes; power can be derived as needed in a later ADR.
