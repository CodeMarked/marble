# ADR-0035: Animation compression helpers and runtime pipeline order (Chapter 12 §12.8–§12.9)

## Status

Accepted

## Context

`[docs/book/chunks/index.md](../book/chunks/index.md)` places **chunk 035** in **§12.8 Compression Techniques** and **§12.9 The Animation Pipeline**. Runtime animation data is often stored in reduced precision; the engine also needs a **documented evaluation order** tying together the Ch.12 modules already landed (clips through skinning).

## Decision

1. Add `[animation/AnimationCompression.hpp](../../engine/animation/AnimationCompression.hpp)` with:
  - translation channel **quantize / dequantize** to `int16` using a configurable `unitsPerMeter` scale and symmetric clamp to `int16` range,
  - **unit-interval** packing (`[0,1]` → 16 bits) for normalized key times or blend parameters in packed assets,
  - a short **runtime pipeline** comment: clip sampling → blend → local post-process → FK → skinning palette (matches `AnimationClip`, `AnimationBlend`, `SkeletonPose`, `Skinning`).
2. Record the same pipeline order in this ADR for traceability outside the header.
3. Verify quantization with `animation_compression_test`.

## Consequences

- Positive: a stable on-disk / network numeric format for translation tracks without pulling a full animation codec.
- Positive: new engineers see how Ch.12 pieces compose at runtime.
- Trade-off: no rotation compression (quaternion shorts), curve fitting, or key reduction yet.
- Follow-up: bit-packed rotation, adaptive quantization per track, streaming clip loader, and exporter tooling.

## Alternatives considered

- **Third-party compression only:** rejected for the chunk; we still want testable in-engine quant/dequant seams.
- **Single monolithic “animation system” object:** deferred; free functions + ADR keep boundaries explicit.

