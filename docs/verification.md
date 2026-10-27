# Native reference verification

This records the TFX C++ baseline inherited by Redux. Rendering now intentionally
differs; see the [Redux changes and checks](../README.md#redux-improvements).
The original-code pixel comparisons below apply to the baseline, not the new
resolution-independent renderer.

Current coverage: 19 October 2026, reference **TFX 2.05, September 1994 CD**.
This is the current coverage summary. [Implementation notes](implementation.md)
retain the chronological research and reproducible subsystem checks; their
older open-work statements describe earlier builds.

## Evidence and boundaries

| Area | Verified coverage | Boundary |
| --- | --- | --- |
| Retail launch/rendering | All 128 retail missions, sixteen complete Arcade frames each, matching captured state and every indexed pixel | Early mission frames, not every later encounter |
| Natural flight outcomes | Successful interception and ground strike; hostile loss, ejection, airspace return, runway/carrier service, refuelling, recording/replay and next-hop training inheritance | Complete controlled sorties where stated; supplied setup/transport inputs remain explicit |
| Persistent campaigns | 25 continuous training, five Flashpoint, three Tour and Arcade journeys; 175 flight returns, 1,095 byte-exact save/reload checkpoints, 3,225 state/stack/save comparisons | Flight outcomes/menu choices supplied at declared host boundaries; media and planners observed rather than painted |
| Live campaign returns | Actual private-window pilot restore, authored loadout, ejection/FINISH, retry and native-byte pilot saves for missions 901, 801, 821 and 841 | Original parser predicts handoff from observed natural results; independent full-frame captures verify flight separately |
| Campaign branch breadth | 680 post-flight segments from 96 mission callers, including death, MIA, discipline, promotion and defeat | Explicit supplied result states; separate from natural sorties |
| Synth music resources | All 81 resources across 104 verified hardware auditions, including separate SCC-1A/SC-55 renders | First traversals and release tails, with independently measured loop boundaries |
| Sustained synth music | FM, LAPC-I and Sound Canvas each match 128 persistent flight/menu lifecycles: 21 min 20 s, 69,914 shared IRQ ticks per mode, retaining phase across all resource changes | Original controller, resource replacement, sequencer and driver execute; physical transport/settling declared |
| LAPC effects | 128 lifecycles, 48,895 exact MIDI events/timestamps, repeated bank loading, overlap, channel exhaustion and mute/resume, with original wrappers and OSL/LA playback | MIDI and exact sample timestamps; firmware audio has separate checks |
| CD transport | Shared/split CUE/BIN, pregaps, physical numbering and EOF; WAV/FLAC exact PCM, Ogg/MP3 independent decoder agreement; 274 native controller steps | MSCDEX transport replaced by the host stream |
| QSound playback | Recovered 8 kHz bank, timer-driven wrapping pan, explicit endpoint clamping, native-rate filtering, rate changes, preemption, additive mixing and finite tails; Debug/Release/ASan/UBSan checks | Analytical fixed-point model with stated policies, not independently verified ST18932 audio |
| Live mixed audio | CD continues through configuration; music mute/restart advances; SB effects, including built-in QSound without a firmware asset, mix and mute independently; one PCM output and clean shutdown | Measurable test-authored stereo track, private silent sink; not an analogue sound-card recording |
| Projectile lifetime | 32,768 original/C++ frames with a 17-slot pool, repeated exhaustion/reuse, dispensers and jettison; exact state/event/RNG hashes | Transformed anchors and deterministic contact losses supplied; allocation, motion and retirement execute |
| FM hardware cleanup | Real initialization and callback partition tests; all 27 reference FM resources retain exact pre-fix PCM/state hashes | Bounded preservation evidence, not proof of every hardware path |

The repeated campaign, projectile and FM checks also pass with ASan/UBSan;
live mixed CD/effects playback additionally checks worker/device lifetime with
sanitizers and leak detection. LeakSanitizer is disabled only where the
read-only GDB observer requires ptrace; ordinary sanitizer checks keep it on.
All desktop integration checks use disposable installations and private X11
windows; audible-output checks use their own silent PulseAudio sink.

## Reproduce the closeout checks

The analytical commands use the sibling `tfx-reverse` research tree, including
its independently checked archive reader, original resources and Unicorn.
Most require the research Python environment. No original game data is committed.
The 213 asset-free regressions pass in development and Release; the same suite
is also checked with ASan/UBSan. Run them with:

```sh
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Campaign and endurance checks:

```sh
python3 tools/check_campaign_journeys.py --report /tmp/campaign.json
python3 tools/check_campaign_flights.py --report /tmp/campaign-flights.json
python3 tools/check_training_engagement.py --campaign --recording
python3 tools/check_music_session.py --mode fm --cycles 128
python3 tools/check_music_session.py --mode lapc --cycles 128
python3 tools/check_music_session.py --mode scc --cycles 128
python3 tools/check_lapc_effects.py --cycles 128
python3 tools/check_projectile_runtime.py --frame-tail --recycle --frames 4096
python3 tools/check_audio_menu.py
python3 tools/check_fm_pcm.py
```

`--build build-san` selects a sanitizer build for these tools. For checks that
run outside GDB, use `ASAN_OPTIONS=detect_leaks=1` and
`UBSAN_OPTIONS=halt_on_error=1`. Optional JSON reports contain counts and scope;
bulky native captures, generated PCM and game bytes remain outside the source
repository. `check_mission_sessions.py` retains original captures for reuse
when a specific later change warrants another corpus sweep.

## Limits and intentional omissions

This evidence does **not** claim that every mission has been won naturally,
every complete campaign has been played through, or every combination of
weather, realism, loadout and AI encounter has been exhausted. Broader controller
coverage and complete natural sorties are deliberately reported separately.
Future specific failures should become reproducible original/C++ comparisons.

QSound now uses the reusable source-included QSound C++ model with built-in numerical coefficient tables. No firmware file, resident DOS driver or physical card is required. The model uses recovered coefficients, default 8 kHz bank selection and the original wrapping pan controller, with explicit endpoint clamping and floor/per-MAC saturation assumptions. Out-of-table DSP addressing, exact rounding/saturation and the hardware sample interface remain unverified against a physical card. Four-input processing, the complete DSP instruction set and VU/control-register emulation are outside this two-input playback implementation. Ordinary SB, Gravis, LAPC effects and music also remain available independently. Lower-RAM
Gravis streaming, unlinked authoring/Inferno material and non-x86 numerical
portability are outside this reference configuration. Windows/macOS builds
remain conveniences; Linux is the verification target. CI remains unconfigured.

QSound's reusable source is pinned and compiled directly into `tfx_audio`; it has no separately installed library or runtime driver. The offline integration test independently derives the PIT/BIOS callback count and DAC sample-hold indices, checks all three host rates 8/44.1/48 kHz, and compares contiguous and fragmented callbacks. It also checks rejected requests, preemption, history persistence and that pre-existing stereo audio is added without being processed by QSound. The standard CTest uses the built-in reference bank and synthetic PCM without external assets. The component separately verifies every included coefficient against the extracted original firmware and its PCM output against independent scalar equations.

```sh
./build/qsound_playback_test
python3 tools/check_audio_menu.py --build build-rel --qsound auto
```

The live check creates a private display, audio sink and installation. It observes CD progress, mute/restart, effects alongside music, a single output device and clean shutdown. The default built-in QSound path is exercised with no CSP/PVL in the private installation. `--qsound off` checks ordinary mono effects separately. Original firmware extraction remains an optional research workflow in the QSound component repository.
