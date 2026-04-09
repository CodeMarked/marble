# ADR-0045: Spatial audio (3D rendering) baseline (Chapter 14 §14.4)

## Status

Accepted

## Context

`[docs/book/chunks/index.md](../book/chunks/index.md)` places **chunk 045** in **§14.4 Rendering Audio in 3D**. With world-space conventions fixed (`[ADR-0017](ADR-0017-three-d-math-conventions-and-vec3.md)`) and PCM/mixing helpers landed (`[ADR-0044](ADR-0044-sound-technology-pcm-layout-baseline.md)`), the engine can express **listener-relative panning**, **distance attenuation**, and a textbook **Doppler** scale without binding to OpenAL, XAudio2, or similar APIs.

## Decision

1. Add `[audio/AudioSpatial3D.hpp](../../engine/audio/AudioSpatial3D.hpp)` under `marble::audio`:
  - `Listener3D` / `ListenerBasis` and `buildListenerBasis` — orthonormal frame from `forward` and `worldUpHint` (`right = normalize(cross(worldUpHint, forward))`, `up = cross(forward, right)`), matching ADR-0017 (+Y up, +Z forward).
  - `stereoPan11` — horizontal-plane pan in [-1, 1] (elevation removed along listener `up`).
  - `distanceAttenuationLinear` (min/max distance piecewise linear) and `distanceAttenuationInverseDistance` (delegates to `[pointSourcePressureScale](../../engine/audio/PhysicsOfSound.hpp)`).
  - `stereoSpeakerGainsLinear` — map pan + per-source distance gain to left/right weights.
  - `radialVelocitySourceTowardListener` and `dopplerPitchScale` — stationary-listener, subsonic sketch using `kSpeedOfSoundDryAir20C` from `[PhysicsOfSound.hpp](../../engine/audio/PhysicsOfSound.hpp)`.
2. Verify with `audio_spatial_3d_test`.

## Consequences

- Positive: gameplay can drive stereo spatialize + simple pitch shift from shared math before a backend exists.
- Positive: formulas stay in one header for later middleware mapping.
- Trade-off: no HRTF, occlusion, reverb, or multi-channel surround pan laws.
- Follow-up: chunk 046+ for engine voice/graph architecture; hardware submission in a dedicated platform module.

## Alternatives considered

- **Third-party spatializer in this chunk:** rejected; keep the milestone testable and dependency-free.
- **Constant-power pan law:** deferred; linear pan is easier to reason about in tests; swap at the mix bus when mastering curves are chosen.

