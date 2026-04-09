# ADR-0044: Sound technology — PCM layout baseline (Chapter 14 §14.3)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 044** in **§14.3 The Technology of Sound**. After continuous and discrete-time math ([`ADR-0042`](ADR-0042-physics-of-sound-baseline.md), [`ADR-0043`](ADR-0043-mathematics-of-sound-baseline.md)), runtime audio touches **how PCM is laid out in memory** (sample rate, channel count, bit depth, interleaved frames) and how **multiple digital sources** combine before conversion to analog. This milestone stays API-agnostic: no device drivers or codec libraries.

## Decision

1. Add [`audio/SoundTechnology.hpp`](../../engine/audio/SoundTechnology.hpp) under `marble::audio`:
   - `SpeakerLayout` (`Mono` / `Stereo`) for documented channel counts.
   - `pcmBytesPerSample`, `pcmBytesPerFrame`, `pcmByteSizeForFrames` for interleaved PCM sizing.
   - `pcmStreamParamsValid` — coarse validation (rate, 1–16 channels, 8–32 bits multiple of 8).
   - `interleavedStereoWriteInt16` / `interleavedStereoReadInt16` for L/R frame packing.
   - `mixAddClamped` — normalized float sum clamped to [-1, 1] as a minimal digital-mix primitive.
2. Verify with `sound_technology_test`.

## Consequences

- Positive: buffer allocation and ring-buffer math can key off shared frame/byte helpers.
- Positive: interleaved stereo matches the usual device callback layout taught in the chapter.
- Trade-off: no surround layouts, planar PCM, or codec framing yet.
- Follow-up: chunk 045+ for 3D audio; later ADRs for hardware backends and voice graphs.

## Alternatives considered

- **Defer until a real audio API:** rejected; the chapter’s “technology” layer is mostly data layout and mixing policy, which we can test without I/O.
- **Planar (non-interleaved) only:** rejected; interleaved stereo is the default teaching path for game PCM.
