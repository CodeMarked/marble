# ADR-0046: Audio engine architecture voice-graph baseline (Chapter 14 §14.5)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 046** in **§14.5 Audio Engine Architecture**. Previous chunks grounded physics/math/PCM/spatialization primitives ([`ADR-0042`](ADR-0042-physics-of-sound-baseline.md)–[`ADR-0045`](ADR-0045-spatial-audio-3d-baseline.md)), but runtime still lacks an **engine-owned orchestration seam** for voices and bus mixing before platform backends.

## Decision

1. Add [`audio/AudioEngineArchitecture.hpp`](../../engine/audio/AudioEngineArchitecture.hpp):
   - `AudioVoice` (active/spatialized flags, dry gain, distance gain, pan, pitch scale),
   - `StereoBus` output accumulator,
   - `AudioMixerGraph<MaxVoices>` fixed-capacity voice graph with `setVoice`, `voice`, `clearVoice`, `clearAll`, `activeVoiceCount`, `mixStereoSample`.
2. Keep mixing policy explicit and dependency-free:
   - voice pan/distance resolved to L/R via [`stereoSpeakerGainsLinear`](../../engine/audio/AudioSpatial3D.hpp),
   - bus accumulation uses [`mixAddClamped`](../../engine/audio/SoundTechnology.hpp),
   - helper `makeSpatialVoice` maps listener/source state to voice params (`distanceGain`, `pan11`, `pitchScale`).
3. Verify with `audio_engine_architecture_test`.

## Consequences

- Positive: gameplay/runtime can stage voices in a deterministic fixed-capacity graph independent of device APIs.
- Positive: chapter-level boundaries are explicit: spatial math feeds graph params; graph feeds backend buffers.
- Trade-off: no send/return effects, no async decode/stream graph, no per-voice envelopes.
- Follow-up: wire backend callback and buffer queue policy in later audio-engine chunks.

## Alternatives considered

- **Go straight to OpenAL/XAudio2 graph objects:** rejected; keep architecture contracts testable without platform APIs.
- **Dynamic allocation per voice update:** rejected; fixed capacity matches existing deterministic container strategy.
