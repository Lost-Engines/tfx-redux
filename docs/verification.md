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

## Redux playtesting regressions

`redux_view_test` checks horizon spans against per-pixel rays, including wide,
banked and near-vertical views. `redux_effects_probe DATA_DIR` checks the actual
missile smoke model at five distances and four resolutions, plus its complete
SSD animation: LOD scales 0, 0.1 and 1 must retain the same particle representation
and lifetime. It also checks identical smoke sample positions across different
update divisions, wider-FOV visibility, circular lights shrinking with distance,
resolution-dependent object minima, and occlusion independent of polygon drawing
order. Actual Eurofighter, F-117 and F-22 assemblies retain their 4K visibility
floor at three distances, four orientations and all three LOD scales, even with
full meshes enabled. `redux_arming_probe`
also checks target resizing.

`redux_hud_probe DATA_DIR [CAPTURE_DIR]` checks the vector HUD at native,
fractional-scale and 1080p resolutions: subpixel motion, antialiased edges,
non-brightening stroke joins, additive colour, cockpit occlusion and all three
weapon layouts. The video and screenshot tests preserve the optical coverage
and blended RGB values; version-1 recordings remain readable. The live recording
check also compares the optical layer and every final RGBA pixel.

`python3 tools/check_window_playtest.py --redux` uses a private X11 display and
GDB as an observer. It checks continued flight without focus, ignored background
input, holding the first Ctrl+Escape safely, a second press ejecting, Escape menu
entry/cancellation, and a 913×617 render target after resizing. Add
`--menu-eject` to check confirmed menu ejection, or `--fixed-resolution` to check
that an explicit target remains fixed when the window changes. Test installs
and pilot/configuration files are disposable. `--mouse-orbit` additionally checks
external camera mouse movement, wheel zoom, reset, focus release and separation from flight
steering. `mouse_orbit_tests` checks pivot preservation, zoom direction and bounds,
fractional input, unchanged FOV and tracking for every supported external view,
including missiles and the ejected pilot.

## Pause input

`pause_input_tests` checks resume keys, held-key suppression, release/repress,
focus changes and modal shortcuts. `python3 tools/check_window_pause.py --redux`
checks the actual window on a private X11 display with a disposable install.
GDB observes simulation ticks and projectile launches: P/Pause stop simulation;
Enter, Space and G resume; holding the Space resume press cannot fire; a fresh
press after release can fire normally. Modifier keys alone leave flight paused.

The same check switches F4/F5/F1 while paused, waits for camera transitions,
compares a settled view across repeated frames, and saves an in-game screenshot.
Simulation ticks and scenery animation calls must stay unchanged throughout.
Redux additionally checks that paused mouse orbit and wheel zoom change the rendered view, that reverse scrolling restores it, and that camera selection resets zoom.

`tools/check_window_designator_mouse.py --redux` checks real mouse movement and both mouse buttons in Z view, confirms that flight receives no mouse input, and checks that F1 restores steering without carrying over camera input. Run it with both `--flight-model arcade` and `--flight-model military-spec` to cover relative steering and the retained mouse stick. It also checks paused designator images for unwanted animation. The test uses a private X11 display and temporary configuration; GDB only observes state.

The Redux check additionally verifies aiming direction, wheel zoom, paused aiming and zoom, and preservation of a locked designation. `render.redux_designator_mouse` covers fractional input, sensitivity at different zoom levels, bounds, lock/release input and view/focus reset. Physical joystick hardware is not exercised by these checks; the existing joystick input route remains independent.

## Mouse steering at low frame rates

The original `0x7459c`/`0x74508` mouse update clamps the previous position before adding the current motion, checks at most one axis per lower/upper pass, and uses signed-word wrapping. A long frame can therefore supply a stick position beyond its nominal −512…511 range. `0x9a104` cubes the resulting mouse demand: a 4,000-pixel/second sweep gives about 4.6 times full aileron demand at 15 updates/second and 8 times at 10. Redux saturates both axes after accumulation, without wrapping; the native numerical controls and their comparison fixtures remain unchanged.

`flight.redux_mouse_bounds` exercises the actual aerodynamic control law with positive and negative sweeps at 10, 15, 30 and 60 input updates/second, diagonal limits, extreme deltas, immediate reversal and unchanged in-range sensitivity. `tools/check_window_mouse_rate.py --flight-model military-spec --fps 10` adds real X11 pointer events on a private display, delaying presentation without changing simulation state. Use `--flight-model realistic` or `arcade`, and `--fps 15` or `60`, for the other checks. The nominal FPS is a presentation cap; the native flight clock can impose a lower update rate. Every injected relative sample must be consumed once, and Military-spec/Realistic must receive bounded stick positions. Arcade's relative-motion path is unchanged.

### Live virtual-cockpit displays

`virtual_cockpit_probe DATA_DIR [CAPTURE_DIR]` compares the continuous fast and
general texture painters, including clipped/out-of-bounds spans, lighting remaps,
transparency and write coverage. It checks unlit opaque-black display texels,
port/starboard mapping, the continuous centre-display seam and all six ordinary
and refuelling canopy models at 960×600, 1080p and 4K. Optional captures use the
real instrument artwork; the benchmark prints isolated canopy costs. A 320-column
source projected through a 256-column atlas to 640 screen pixels must preserve
exactly two pixels per column, catching intermediate resampling. Both general
and fast texture paths are compared with direct mappings and texel overrides.
The probe also exercises changing/clearing gear, warning, autopilot, clock,
engine, fuel, throttle, reheat, chaff and radio regions, and repaints cached
engine lamps/reheat pips across six fresh canvases. The existing native HUD
comparison still matches all 504 complete F1 pixel/state frames.

`python3 tools/check_window_virtual_cockpit.py --data-dir DATA_DIR` launches
mission 703 on a private X11 display with temporary configuration and no audio.
It cycles the MFDs in F10, exercises scene-camera pages and L radio replay, checks that paused images
and flight inputs stop changing, and switches between paused F5, F1 and F10.
GDB observes execution without changing simulation state. Screenshots go to
`/tmp/tfx-virtual-live` by default (`--captures` selects another directory).

## Redux cockpit and distant effects (28 October 2026)

Fresh installations now select Military-spec and keyboard controls in both
ports. Saved configuration still takes precedence. Redux starts in F10;
keyboard/joystick flight permits captured mouse head movement in the virtual
cockpit, including while paused. An isolated Xvfb run checked F1/F2/F3/F4/F10,
paused head movement and configuration without touching the working install.

The HUD layout now fills the glass with a small border. `redux_hud_probe`
measures the actual Military-spec layouts for all three aircraft against the
projected glass at two resolutions and three FOVs, as well as checking optical
alignment and clipping. Fresh configurations enable realistic G effects and
wind; preference-file and CD-install checks also verify that existing choices
are preserved. A silent private Xvfb flight of mission 702 at FOV 35 and 64
confirmed that wind reaches the live physics input and G effects use the
Realistic setting; F10/F1 captures were inspected. Redux retains the wind
choice across mission initialisation and servicing, where DOS clears it.

The collimated HUD uses the six inner frame vertices shared by all six retail
cockpit/refuelling models. `virtual_cockpit_probe` verifies these against the
actual decoded geometry. `redux_hud_probe` checks the glass aperture and optical
boresight at 960×600 and 3840×2160, FOV 10/64/100/140/170°, left/right turns, a side-on view and a rear
view. Ordinary flat HUD layouts and opaque-black live MFD texels still pass.

Transparent texture index zero is rejected before distance remapping, including
for point occlusion. Previously its darker rows selected fixed UI colours,
causing pale/green areas around distant crash decals. Aircraft minimum-size
selection now measures projected surface area: the previous bounding box missed
thin MiG-21, MiG-29 and F-16 silhouettes that covered zero or one pixel. The
real-model visibility probe includes these aircraft across the LOD transitions.

Redux trail emission uses the actual camera instead of the DOS 4000-unit
player radius, and the 8000-unit cleanup radius no longer deletes visible distant
puffs. Full coordinates avoid word-wrap aliases. The 200-slot pool and authored
animation lifetime remain bounded; original trail fixtures still pass.

### Missiles around a wreck

No guidance rule was changed. `check_projectiles.original` executed the original
2.05 motion callbacks for 15,600 additional states, matching C++ state hashes
throughout. Four air-to-air weapons were tested against a stationary point at
three initial headings. A further AMRAAM case compared targets at ground level,
one world unit above it, and 100 units above it. The ground-level case first
reached y=0 at frame 160; the one-unit case circled until frame 629 before ground
contact. The 100-unit case circled without touching y=0 and exhausted its flight
stage at frame 761. Each frame supplied four timer ticks. These isolated motion
runs exclude terrain geometry and proximity contacts; they do not claim to
reproduce a particular playtest wreck.

To repeat the latter cases, feed the following row to `original(data_dir,row,False)`
from `tools/check_projectiles.py` and compare its hashes with `projectile_probe`:

```python
# target_y is 0, -256 or -25600; coordinates are Q8.
row = [4, 2400, 4, 0, 0, 0, 0, 5000,
       1000000, -25600, 1000000, 0, 32768, 0, 400,
       1005000, target_y, 1005000, 0, 0]
```

The runtime separately destroys missiles at y>=0 and on terrain contacts.
`projectile_tail_tests` now explicitly checks ground impact and retirement, as
well as extended-distance smoke dispatch. A missile need not touch the ground
merely because its target has crashed; the native guidance follows the target
position until impact or flight-stage expiry.

## G effects and breathing

`tools/check_g_effects.py` compares 8,192 complete original G-controller updates;
`tools/check_palette_lighting.py` compares 4,224 original palette updates across
33 environments, including all colour bytes and breathing requests.

`tools/check_window_g_effects.py --data-dir /path/to/TFX` checks the running game
in a disposable installation, private X11 display and silent PulseAudio sink.
It supplies +15, −8 and +1 G at the controller input using GDB; flight dynamics
are outside this integration check. The real controller, palette painter,
sample scheduler and PCM device then run normally. It checks full blackout and
redout pixels, all three urgent breathing requests, audible PCM, recovery,
paused camera redraws and shutdown. Ordinary key-click sounds are allowed
while changing paused views. Add `--redux` to check the higher-resolution
virtual cockpit and vector HUD. No saved configuration or pilot is used.

## Runtime Redux graphics

`configuration.redux_graphics` exercises the shared command-line/menu/file
validation, invalid and oversized values, arbitrary resolutions, zero LOD and
infinite distance, Apply/Cancel/Defaults, exact floating-point persistence and
keeping the original `TFX.CFG` separate. A temporary build of the preceding
Redux revision matched all 240 tested existing-menu states and complete frames
with the graphics extension disabled. The direct DOS pixel comparison still
reports the earlier Redux Eject button at the bottom right; it is not a pixel
reference for the extended menu.

`python3 tools/check_window_graphics.py --data-dir DATA_DIR` uses a private Xvfb
display and disposable install. Real clicks and typing change size, FOV, draw
radius and LOD while GDB observes the actual scene arguments. It checks invalid
edits, cancellation, pause retention, switching to automatic resolution and
resizing the window, saved preferences on a second launch, individual CLI
overrides, Defaults/window fit and clean exit. It writes menu and flight
captures under `/tmp/tfx-graphics-menu` by default.
