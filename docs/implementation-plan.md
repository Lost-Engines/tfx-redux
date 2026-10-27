# Native implementation closeout

Reference: **TFX 2.05, September 1994 CD**, native C++26, Linux and the original
software renderer. The [verification matrix](verification.md) is the current
coverage summary. [Implementation notes](implementation.md) retain the detailed
chronological findings and commands.

## Completed verification pass

- Replace DBOPL's null-object member-offset calculations with standard-layout
  offsets; define its signed fixed-point wraparound and preserve all 27 FM
  resource PCM baselines.
- Exercise persistent training, Flashpoint, Tour and Arcade control flow with
  repeated byte-exact native save/load, retries, safe/repeated ejection,
  discipline, promotion, defeat and wrapped scores.
- Bind actual Flashpoint/Tour flight returns to original parser predictions
  and native pilot saves, alongside the naturally completed training engagement,
  recording/replay and next-hop inventory inheritance.
- Extend original 32/16-bit synth controller/driver comparisons over retained
  timers, repeated resource reuse, cue changes, mute/engine state and menu return.
  Repeated LAPC effect-bank overlap/exhaustion has separate original execution.
- Stress bounded projectile allocation, rejected launches, retirement and reuse
  over long original/C++ sequences, retaining exact random/event/state comparisons.
- Check real mixed CD/effects playback through live configuration and shutdown;
  fix the menu redraw that repeatedly restarted the CD track.
- Run asset-free regressions and relevant lifetime checks under sanitizers;
  keep native packages minimal, fetching the reference CD and ROMs separately.

## Completion scope

The pass closes the identified implementation and verification tasks; it does
not establish exhaustive complete-game equivalence. Supplied campaign-result
branches must never be described as naturally completed sorties. Full-frame
original comparisons, naturally played integrations, subsystem comparisons and
C++-only stress checks retain their separate claims in the matrix.

When a specific unverified interaction is identified, capture the original with
explicit controls, timer intervals, inherited campaign state and resource hashes.
Compare every retained state, indexed pixel and displayed palette, reduce the
first divergence, and preserve a compact regression. Request interactive help
only when a prepared question cannot be resolved analytically.

## Deliberately outside this pass

- Exact original-hardware QSound verification; the built-in analytical model is implemented.
- Lower-RAM Gravis streaming and unlinked authoring/Inferno paths.
- Windows/macOS verification and non-x86 numerical portability; Linux is the
  authentic reference target ahead of a separate eventual browser project.
- CI and automatic releases, which remain unconfigured as requested.

No general architecture rewrite or additional user-facing feature is needed
for this closeout. Keep subsequent work driven by a concrete discrepancy or a
separately agreed feature.
