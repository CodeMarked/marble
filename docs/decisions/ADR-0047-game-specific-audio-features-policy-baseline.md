# ADR-0047: Game-specific audio features policy baseline (Chapter 14 §14.6)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 047** in **§14.6 Game-Specific Audio Features**. With core voice graph and spatialization seams already present ([`ADR-0045`](ADR-0045-spatial-audio-3d-baseline.md), [`ADR-0046`](ADR-0046-audio-engine-architecture-voice-graph-baseline.md)), gameplay needs policy helpers for **one-shot trigger gating**, **deterministic variation selection**, and **mix-category ducking** without coupling scripts directly to mixer internals.

## Decision

1. Add [`audio/GameSpecificAudio.hpp`](../../engine/audio/GameSpecificAudio.hpp):
   - `MixCategory` + `CategoryGains` and `gainForCategory`.
   - `OneShotState`, `canTriggerOneShot`, and `nextVariationIndex` for cooldown + deterministic variation cycling.
   - `OneShotSpec` + `triggerOneShot` mapping game events into `AudioVoice`.
   - `applyVoiceOverDucking` to attenuate music/SFX while voice-over is active.
2. Verify with `game_specific_audio_test`.

## Consequences

- Positive: gameplay-facing audio events can be authored as policy data (`OneShotSpec`) with deterministic behavior.
- Positive: cooldown and variation logic are centralized and testable.
- Trade-off: no priority stealing, no probability tables, no surface/material routing yet.
- Follow-up: connect these policies to gameplay tags/state machines and backend scheduling.

## Alternatives considered

- **Keep game-specific logic in scripts only:** rejected; chapter emphasizes shared runtime patterns, and tests are easier with C++ policy helpers.
- **Random-only variation without cursor:** rejected; deterministic cursor keeps behavior reproducible in tests/replays.
