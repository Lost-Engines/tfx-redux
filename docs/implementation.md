# Reconstruction and verification

Reference: TFX 2.05, September 1994 CD. Native offsets refer to `TFX.DAT`, SHA-256 `2cc41a39fa95a2c9fd883c4220d7bdd9cca00c543c89b019eb270763f59d1d9a`.

Current scope and evidence are summarized in the [verification matrix](verification.md).
The sections below retain the implementation history; later results supersede
earlier open-work statements.

## Integrated reference check

All **128 retail missions** match the original launch and their first sixteen
Arcade frames in every captured state and indexed pixel: **2,048 complete
frames**. Controlled comparisons also cover targeting, cannon and missile
release, a real aircraft kill, objective completion, recording and mission
return. The successful training sortie agrees through all **233 frames** and
the original recovery/FINISH return, including final score and campaign values.
The verification sections below distinguish these complete sessions from
isolated subsystem checks and list the remaining gaps. This does not establish
complete-game equivalence.

The original presentation variables, DOS devices, external timer, silent audio
and host memory remain explicit capture inputs. The original mission,
environment, renderer, arming and AI code executes. `--flight-model` selects
any of the four models, and `--output` retains the private frames and observer
log. `--strict-pixels` requires every captured image to match, not just the last.

The current asset-free regression suite is listed in the verification matrix. All 128 retail
missions also compose 64 development frames and 16 sanitizer frames without
preview warnings. Real private-window controls exercise firing, MFDs,
missile cameras, designator use, ejection and an automatic FINISH handoff.

The sections below record successive implementation stages. Their early
limitations describe those stages; later native checks supersede them.

## Initial reconstruction boundaries (historical)

* `DID.DAT` index, native filename substitution/hash, raw and RA v2 resources load directly from the selected installation.
* All 3,488 reference archive entries compare byte for byte against did-unpack (2,416 RA-compressed and 1,072 raw); malformed input is tested without game data.
* All 1,012 model programs decode directly from the archive. The C++ VM matches the checked inspection interpreter in 2,042 detail/state cases (253,334 vertices). This establishes program interpretation, not native camera/raster equivalence.
* All 703 valid SSD definitions match their decoded initial parts, anchored draw calls and parameter streams. All 1,440,000 cells across 36 grid/lighting combinations match their tile IDs and deterministic inspection variant choices. Actor initialiser random side effects are not yet simulated.
* Original x86 calls independently verify 1,332 condition predicates, 205 component matrices and 64 world random updates. All 1,024 regenerated sine samples match the executable, including the -32766 negative peak.
* Native comparisons cover 1,260 animation-clock frames, 126 completed parameter-stream updates and 48 multi-vertex jitter cases. A non-returning synthetic stream is rejected explicitly rather than counted as a successful update.
* All 128 retail mission scene selections, signed starting coordinates and optional aircraft assignments match their source metadata, and every initial scene renders successfully. Five original-code captures verify the separation of regional terrain from weather/lighting configuration.
* A headless timing replay matches 4,096 original-code frames, including timer polling, filtered float/timestep double bits, previous-frame animation gating and ordered projectile substeps. This is a subsystem sequence with declared external inputs, not a complete flight session.
* The complete native mission-script loader agrees with typed C++ INIT results for all 128 retail missions in two prior-state contexts (256 setups), including target/message text, speed bits, timing, flak and scenery-event declarations. Resource/actor creation and FINISH execution are outside this capture.
* All 128 retail FINISH programs agree with original execution across 8,192 supplied result states: scoring, signed-word conditions, target flags, branch/return handling and campaign statistics. Live combat/mission integration is described below; persistent campaign/front-end execution remains outstanding.
* Carried-store mass, drag, both moment contributions and aircraft-specific fuel capacity match the original x87 routine bit for bit in 1,197 loadouts, including every store/aircraft/slot combination and fractional mixed mounts.
* Arcade aircraft motion is translated and checked against original execution (details below); audio and complete flight sessions remain outstanding.

## Verification approach

Each subsystem needs evidence from the original code: controlled calls with declared memory/inputs, output and side-effect capture, then the same cases through C++. Preserve timing, random seeds and external callback results for sequence tests. Integer results should match exactly; numerical tolerances for x87-derived calculations require explicit justification and checks of resulting branches/state transitions.

Use isolated native calls first, subsystem sequences next, and instrumented DOSBox traces for behaviour requiring complete mission context. Research-viewer approximations are inspection aids, not proof of native gameplay behaviour.

The developer probe streams decoded resources to the reference checker over a pipe. Nothing is extracted into the installation or copied into the repository.

## Reproducing the analytical checks

```sh
python3 tools/check_native.py --build build --data-dir /path/to/TFX
python3 tools/check_shapes.py --build build --research ../tfx-reverse
python3 tools/check_world.py --build build --research ../tfx-reverse
python3 tools/check_world_selection.py --research ../tfx-reverse
python3 tools/check_mission_scenes.py --build build --research ../tfx-reverse
```

`check_native.py` and `check_world_selection.py` need Unicorn and the original reference executable. They execute bounded original routines in isolated x86 memory with controlled stack/register/global inputs; DOS, game startup and installation files are not modified. The shape/world comparisons additionally need the research tree and Node.js, and explicitly distinguish inspection comparisons from native execution.

## Integrated scene and flight display

The executable loads regional geometry from the mission’s `world` and palette/night settings from its separate `type` declaration. The original world loader truncates filenames to eight characters, so `yugoslavia` selects `lev/yugoslav.*`. The similarly named weather grids remain available through the developer `--grid` override; they are not substituted for the actual regional terrain. This also avoids the out-of-table IDs in the retained `colum_d` and `somal_st` grids without inventing a repair.

Numeric start Y values already use signed native world coordinates. `landed` is retained as a separate state; only the inspection camera substitutes an elevated viewpoint. The literal scene metadata retains an omitted aircraft as unspecified. Original-loader execution now establishes that INIT resets the runtime aircraft to Eurofighter before reading any explicit `plane` command; it is not inherited wholesale from the preceding mission.

The executable loads the original palette, complete SSD model parts, native world scales, tile headings and anchored placements. Whole models paint in camera-depth order plus the signed header bias; each model retains its authored primitive sequence and facing branches. CPU drawing keeps palette indices through flat/shaded spans, affine textures, transparent index-zero texels, lines, fixed-size night lights and sized circles. Original circle tables are read from the selected 2.05 executable as data; no game instructions run inside the application.

Native comparison captures verify all nine complete circle rasters, 27 drawing-mode interiors (43,200 pixels), three procedural seed transitions, 44 rounded-corner recurrences and 1,280 Gouraud samples:

```sh
python3 tools/check_raster.py --build build --research ../tfx-reverse
```

The direct flight view uses the player’s verified Q15 basis and floating projection with six clipping planes. The developer inspector retains its free camera. Neither path claims full native renderer fidelity; its general polygon boundaries use half-open spans, and its scene collection currently uses the nearest polygon detail level. Native fixed-point world-camera setup, exact clipped raster boundaries, runtime palette remaps/fades, weather/horizon effects, dynamic entities and actor-bound textures still need implementation and verification. The simplified horizon and camera movement are explicitly inspection aids. Original actor initialisers can consume additional random values; current tile variants use the checked deterministic inspection sequence.

The display follows Darker's GLFW/OpenGL 2.1 presentation approach: nearest-neighbour upload of a CPU image, 4:3 aspect correction, integer enlargement and a floating window fitted to the work area. Wayland sizing obtains the compositor's maximised area, restores the window and chooses the fitting scale. OpenGL performs no game-world drawing. The first scene is presented before the focus gate: Wayland needs an attached buffer before it can map and focus the window, so waiting for focus first deadlocks startup. Automated Xvfb tests clear `WAYLAND_DISPLAY` so they cannot accidentally open on the user's desktop.

## Scenery animation

Every visited cell owns its part parameters and stream cursors; cached definitions remain immutable. The original PIT divisor gives 54.6204 timer ticks per second. The native signed-word gate subtracts seven only when the accumulator is greater than seven, permits at most one animation update per displayed frame, and clears an excessive backlog. This gives a nominal 7.8029 animation updates per second, with the original frame-rate dependence retained.

Parameter programs preserve byte wrapping, comparisons, relative jumps, held endpoints and per-definition selector eligibility. Animation events now execute the instance’s SSD entry and resume the same selector immediately, including stream replacement. Vertex jitter uses its own native ADD/ADC sequence and a seed held between animation updates, independently of terrain-variant randomness.

These checks establish the local clock, interpreter and jitter arithmetic. Full actor visibility/update eligibility, mission events, random consumption by original initialisers and dynamic actors remain separate work. The runtime now executes entry 1 for its current draw assembly, including damaged model and overlay selection.

## Mission declarations

```sh
python3 tools/check_initialisation.py --build build --research ../tfx-reverse
```

`missions::initialise` produces typed starting state, angles, aircraft/loadout selection, targets, completion declarations, flak regions, timed messages and scheduled scenery events. Direct mission entry validates and uses this setup, including native actor population creation. It stops before FINISH; it does not simulate the declared conditions or events. Unsupported INIT commands fail explicitly.

The native comparison executes original `0x7a4f8`, including its reset sequence, label search, command dispatch, text readers and numeric handlers. Only decoded-source delivery, allocation/release, environment asset setup and the optional saved-start request are intercepted. Those boundaries are declared in the capture. Fresh and seeded-state runs cover every retail script. Omitted `plane` resets to Eurofighter and angles reset to zero; omitted speed/loadout and a `landed` start's untouched height retain supplied prior values. The caller's complete initial state and any subsequent saved-start override remain to be reconstructed.

Text keeps native CRLFs while removing leading indentation per line. Authored seconds use **54 ticks**, distinct from the launcher's actual 54.6204 Hz clock. Target cells become centred world positions using the same 2,048-unit cell contract. Native target/flak/message/event limits are named beside their owning structures. SETBOB stores an SSD event entry and the target cell coordinates: native `0x70db0` resolves its handle and `0x70e44` dispatches the entry after the scheduled time. Static terrain targets now resolve at startup, and the live mission timeline dispatches their entries at the original frame boundary. Attached scenery actors remain a separate integration path.

## Mission result rules

```sh
python3 tools/check_rules.py --build build --research ../tfx-reverse
```

`finish_program` compiles retail FINISH statements into typed predicates, assignments, branches and returns. Result counters and target life states are explicit inputs; campaign variables are a separate declared table. This permits analytical mission-result checks before aircraft, weapons and damage produce those inputs. It is not a substitute for subsequent end-to-end combat comparisons.

The reference executes original `0x7ae80`, its parser and variable lookup, and its target destroyed-flag reader. Synthetic target records and their handle resolution are supplied; optional saved-start persistence is intercepted. The check compares all seven supplied campaign variables after every case, including common statistics updated outside the script. Each of the 128 retail programs receives 64 cases, varying completed state and friendly losses independently of enemy counts, plus target combinations, signed-word boundaries and score overflow. Generated captures can be retained with `--record build/native-rules.json`.

Several native behaviours require deliberate preservation:

* Numeric predicates compare the **signed low 16 bits**, while assignments and score accumulation use wrapping 32-bit values.
* AND/OR terms evaluate left to right without conventional precedence or short-circuiting.
* The common finish handoff credits `pairscore` before the authored script. Most retail scripts add it again; the reconstruction retains both additions.
* RETURN immediately ends the result program. The existing mission-result variable survives if no RETURN changes it; its final nonzero value controls the completed-mission counter.
* Original brace cleanup can skip a following sibling block after a braced IF. This matters in `miss605`: with `completed == 1`, a failed inner `eplanes_down == 10` test skips the next `eplanes_down > 0` block. The native result is zero in the captured partial-success cases. A conventional structured interpretation would award a different score/result.

These claims cover the retail grammar and the declared original-call boundary. Other retained authoring scripts and broader parser dialects remain separate work. The class does not write pilots, run campaign graphs or infer combat outcomes.

## Carried-store flight effects

```sh
python3 tools/check_stores.py --build build --data-dir /path/to/TFX
```

The ten-slot loadout has explicit occupancy, mass, drag, tank classification and mount position. `flight::accumulate_stores` preserves the original slot order, double stores after each contribution, and extended intermediates within each contribution. The two moment groupings retain neutral A/B names until the complete flight-coordinate convention identifies their physical axes. Vectorstorm supplies the corresponding squared vector lengths.

The original routine `0x95dc8` executes without intercepted dependencies. Tests use all 27 original store records, all three native aircraft presets' mount positions, each of the ten slots, empty loadouts and fractional mixed mounts: **1,197 exact stored-bit comparisons per build**. A tank sets the presence flag for any aircraft, but the capacity multiplier applies only to Eurofighter. The subsystem does not yet apply changing stores to an integrated flight trajectory.

The current arithmetic backend requires `long double` with a 64-bit significand, matching the controlled x87 precision used in these captures. Other formats fail explicitly if this subsystem is called; its unit test reports a platform skip. Native scene inspection does not call it. A portable equivalent and the original startup FPU environment still need to be established before claiming flight fidelity on other architectures. Ordinary `double` is not substituted silently.

## Local build validation

Both RelWithDebInfo and Release have been built with GCC 16.2 in C++26 mode. An AddressSanitizer/UndefinedBehaviorSanitizer build passes all six boundary suites, all 128 initial mission scene renders and the byte-exact 3,488-resource comparison. Isolated X11 desktop smoke checks cover the 640×480 scale-2 window, keyboard exit and exclusion of developer options from Release. Other desktop platforms have not yet been built or tested.

The optional `python3 tools/check_wayland_window.py --build build --data-dir /path/to/TFX` regression check starts a private headless Sway compositor and verifies a visible initial frame and clean window close at automatic and explicit scaling, without a keyboard device or initial focus. It does not connect to the desktop session. The previous startup order times out in this check before any scene buffer is submitted.

## Simulation timing and headless replay

```sh
python3 tools/check_timing.py --build build --data-dir /path/to/TFX
```

This check needs Unicorn and the original September 1994 executable; it does not need the research tree. `tools/native_reference.py` relocates its LE objects, supplies flat selectors and a controlled x87 environment, and requires each call to reach its return address within an instruction budget. Unmapped dependencies and DOS interrupts fail explicitly. Every intercepted routine has a recorded reason. `--record build/native-timing.json` retains the generated inputs, states and dependency list in ignored scratch space.

The C++ clock consumes unsigned launcher counter readings without blocking the host event loop. It preserves counter and accumulated-word wrapping, the two-tick minimum, Military Spec's four-tick cap and the general eighteen-tick cap. Realistic takes an additional counter reading even when the first polling loop has already accumulated four ticks. The four `MODEL` indices select native codes Arcade=0, Simple=2, Realistic=6 and Military Spec=1; configuration index and runtime code are separate concepts.

The native frame filter starts at 4.0, weights its preceding float by three and the new interval by one, and rounds at the float store. Its physics timestep multiplies unaccelerated ticks by the exact value of the original `0.02f` constant. Time warp multiplies world elapsed by five after those calculations, subject to the original inhibition word. It must not be implemented by multiplying every subsystem's timestep uniformly. The x87 control word is currently a controlled `0x037f`; a complete DOS startup capture remains necessary before generalising numerical conclusions to the flight equations.

`tfx_replay` is a developer target, built with the tests. It consumes an initial counter followed by frames containing the `MODEL` index, warp/inhibition inputs and timer readings. It reports exact stored numerical bits, animation eligibility/remainder and projectile substeps. Scenery uses the preceding frame's elapsed word, seeded with five at session entry. The native projectile wrapper calls motion then contacts for each chunk of at most ten; even a zero interval performs one pass and the wrapper restores the original elapsed word afterwards.

The reference check executes the original clock, animation gate and projectile wrapper. The launcher counter source is supplied, and projectile motion/contact callbacks are intercepted to record their order and intervals. Aircraft physics, contact outcomes and full session initialisation are **not** claimed by these comparisons.

## Player attitude and Arcade flight

```sh
python3 tools/check_attitude.py --build build --data-dir /path/to/TFX
python3 tools/check_arcade.py --build build --data-dir /path/to/TFX
```

The player's Q15 basis, local rotations, sequential cross-product normalisation and quantised angle extraction follow the native routines. Comparisons cover 4,132 directions, 6,144 rotations, 2,048 normalisations and 2,048 angle extractions. The original atan thresholds are irregular; regenerating a mathematical tangent table changes the resulting angles. World-axis turns transpose the basis before/after local rotation.

The native Arcade path (`0x8dc50`) now supplies aircraft-specific thrust/speed envelopes, afterburner, fuel consumption, filtered control deltas, stall recovery, coordinated turns, motion, gear contact and ceiling limits. One world unit represents eight feet. The numerical translation retains float/double storage boundaries and extended intermediates; it currently requires a host `long double` with a 64-bit significand, as the stores calculation does.

The verifier runs the complete original player initialiser (`0x8c184`, including the aircraft presets) and engine/start setup (`0x8c318`) before multi-step airborne flight. Across all three aircraft, 4,608 steps agree exactly in stored position, integer pose, matrix, control filters, angular remainders, speed limits/response, fuel and angles, including stall/ceiling recovery and afterburner transitions. Host mouse input and scenery/audio/HUD notifications are declared capture boundaries. The FPU control word is the harness's controlled `0x037f`; this is not yet an independent DOS startup capture. The verifier now executes the complete ordinary Arcade player callback (`0x8f56c`) rather than stopping at the inner flight routine. It supplies timer readings and mouse samples, disables automatic recovery explicitly and intercepts warning/audio/scenery output. The automatic recovery controller can select another numerical path even in Arcade mode; it remains separate work. Other flight models, complete collision queries and missions remain unfinished.

`flight.native_arcade_sequences` replays the saved native trajectory hashes without original files or Python. The full comparison tool regenerates and checks all state words; `--save-fixture tests/reference/arcade.txt` updates the compact test fixture only after successful agreement.

## Mission actor populations and surface routes

```sh
python3 tools/check_actors.py --build build --research ../tfx-reverse
python3 tools/check_surface.py --build build --research ../tfx-reverse
```

Compiled programs, commands and entity profiles load from the original archive into typed records. Native initialisation comparisons execute the original dispatcher, profile lookup, pool management, copying and formation placement for all **192 programs / 3,572 individual actors**. Host allocation and SSD creation are declared boundaries. The 50-slot allied/other air pools, single 50-slot surface pool and ten-member group limit follow the executable. Wingmen copy the **preceding member**, so formation offsets accumulate; independently placing every member relative to the leader is incorrect. Actor positions use eight fractional bits and speeds use sixteen. Another 4,096 captures verify wrapped movement, negative-angle table phases and vertical clamps.

The scene owns separate actor assemblies and local animation state, with poses supplied by simulation state. Aircraft/groups execute their route, formation and combat controllers, described below. Surface units execute their native route/controller stacks and update their rendered positions. Their ordering follows the compiled program/native surface pool; `0x75784` visits each once per elapsed AI tick.

Surface comparisons cover all **815 units × 128 ticks = 104,320 updates**, checking every intermediate position, heading, speed, steering/acceleration rate, controller count, wait, stack, cursor and flag via sequence hashes. The supplied context contains no eligible combat targets: nearby weapon selection, scenery-target lookup and radio delivery are declared boundaries. This establishes route/movement behaviour, not firing, damage or live sensor selection. Those requests are explicit outputs from the controller; only radio delivery is currently connected in the windowed integration.

Three compact synthetic-original fixtures add **90,000 updates**, including long travel through a waypoint, acceleration/deceleration, waits, radio and looping branches. `actors.native_surface_sequences` replays those without game files; `check_surface.py --verify-fixture tests/reference/surface.txt` checks their complete inputs and hashes against original execution again. The ordinary scene startup sweep also continues to pass all 128 retail missions with their actor populations present.

## Direct-flight integration limits

The default launch is mission 501. The window now drives player motion through the native timer gate and hands its basis/position to the software camera. It does not render repeated flight frames while waiting for the minimum timer interval. Pause/focus loss discard host mouse deltas and release capture. Keyboard steering is still a provisional adapter to relative native control inputs; it is not yet the complete DOS keyboard/joystick acquisition path.

Live readouts use the original 51-glyph miniature font, read directly from the required reference executable along with its character/advance tables. Their current layout is diagnostic telemetry, not a reconstruction of the complete HUD/MFD layout. Scene inspection remains behind `--inspect`; `--flight-steps` advances simulation for a headless capture.

Gear, engine, air-brake and brake-chute controls are connected for ordinary Arcade flight. The chute state is distinct from engine damage. Ground starts use the native runway/carrier fallback classification. The full damage/death lifecycle, moving-object collision, air combat/projectiles, weapons/sensors, the other numerical flight models, automatic recovery, cockpit/MFDs, campaign/menu/persistence and sound still need integration before this is a complete playable mission.

### Ground-start and takeoff verification

```sh
python3 tools/check_ground_flight.py --build build --data-dir /path/to/TFX
```

Another **9,216 steps** check runway (height 0) and carrier-fallback (height −20) starts, with engines off/on, for all three aircraft. Engine-on sequences are required to finish airborne without a collision. The original initial-placement prologue executes before supplied engine/control changes; landed gear indication comes from the native HUD caller at `0x7e047`. This exposed the original 120-world-unit runway setback, the low-speed-ground-contact flag, the upright-basis landing test, automatic Eurofighter gear retraction and the post-motion contact clamp. A ground start's first placement can reset engine commands, which is why its control boundary must follow that prologue.

Surface height/contact and starting classification are supplied inputs to this comparison. It does **not** verify complete flight sessions with live terrain; terrain queries are checked separately below. Automatic recovery remains disabled in these traces. `flight.native_ground_sequences` replays the saved native state hashes without original files. The runway/gear/contact corrections are shared by headless and interactive flight.

The SSD swept-box primitives (`world/collision`) reproduce the native three projected rectangle tests, including directed tangencies and the original diagonal transform. `tools/check_collision.py --data-dir PATH` compares 11,250 normal, reversed, diagonal and boundary cases against 2.05; the small boundary regressions also run through CTest. The scene now also implements the static terrain dispatcher, checked separately below.


## Stateful scenery and terrain collision

`world/instance` owns mutable parts, custom words, animation cursors, damage flags and object health separately from shared immutable definitions. It executes the authored entry points, conditional branches and helpers. Damage can change models and draw assemblies, start effects, and replace running parameter streams. Stream-triggered events resume the current selector in the same animation tick. Original health comparisons, wrapped subtraction and already-destroyed stream suppression are retained.

```sh
python3 tools/check_instances.py --research ../tfx-reverse
python3 tools/check_world_collision.py --research ../tfx-reverse
```

With Unicorn installed, these compare **19,589 persistent states across all 703 definitions**, then **4,800 complete native terrain queries** across Yugoslavia, Colombia, Libya and Somalia. The latter execute the original cell selection, negative-remainder wrapping, quarter-turn conversion and collision entry, including map seams. Host model handles, draw submissions, collision callbacks and anchor-distance inputs are declared boundaries in the instance test. The world test supplies independently initialised instance resources and records solid contacts; it does not execute damage to the querying aircraft. CTest also covers independent instances, strict-negative damage thresholds, restoration and an animation event that replaces its own stream.

Interactive flight now uses live terrain/building queries and carries the resulting runway surface into the next physics step, following native update order. A solid contact currently stops flight with the diagnostic collision state. Moving-object contacts, full aircraft damage/death and restart, attached-object event resolution, visibility/update eligibility and complete-session comparison remain unfinished. Static instances are still created on first use. Authored temporary-effect requests now reach the live pool, as detailed under secondary scenery blasts below; legacy offset requests remain separate; projected ground-shadow submissions are distinguished from normal model draws and await the shadow renderer. This is not a claim that the whole SSD/render lifecycle is complete.


## Live mission timeline

`missions/timeline` retains the unsigned mission clock independently of the authored time of day. Native messages run at the start of the frame with an inclusive deadline; scheduled SSD events run after rendering and clock advancement with a strict deadline. Both retain authored order and execute once. Off-screen terrain cells named by SETBOB are prepared before flight so their mutable state does not depend on visibility. Message drawing currently uses the diagnostic HUD text adapter; native message layout/lifetime and attached-object resolution remain separate work.

```sh
python3 tools/check_timeline.py --data-dir /path/to/TFX
```

The original `0x71b40` and `0x70e44` agree across **4,096 frame boundaries**, including equality, unsorted event times, zero steps, signed-looking authored times and unsigned wrapping. Message display, SSD dispatch and player SSD refresh are declared capture boundaries; full SSD entry execution is verified separately above. The compact original-result fixture also runs in CTest without game files.


## Airborne controller arithmetic

`actors/air_motion` supplies typed aircraft performance, pitch/turn/speed commands, altitude capture, controller-step shortening and bank-coupled Q8 motion. It preserves wrapped angle/product arithmetic and emits typed afterburner SSD events on display-state edges. Other flight models can use the original six-class turn-envelope data read from the external executable; Arcade retains its profile/speed rule.

```sh
python3 tools/check_air_motion.py --research ../tfx-reverse
```

**9,216 native cases across all 29 aircraft profiles**, plus **576 test-authored cases**, agree for the native controller primitives, envelope selection and motion step. CTest replays the latter without original assets. The afterburner event callback is a declared capture boundary. These checks establish the numerical controller foundation; the waypoint and formation dispatcher is now connected to the live scene as described below; combat acquisition and manoeuvres remain separate work.


## Airborne waypoint runtime

`actors/air` owns mutable compiled programs once per record and keeps each aircraft's cursor, action stack, control state, formation relationship and burst state separately. Group members share command mutations, so a radio instruction delivers once for the group. Aircraft now move in the live game, including waypoint branching, formation catch-up, altitude/speed commands, ground waits, takeoffs and afterburner animation events. Native ground-attack commands produce explicit projectile requests; actual projectile effects and air-to-air acquisition are still unconnected.

```sh
python3 tools/check_air.py --research ../tfx-reverse --ticks 128
python3 tools/check_air.py --research ../tfx-reverse --ticks 512 --intact 1
```

All **192 programs** agree for **352,896 aircraft ticks** with unavailable ground targets and **1,411,584 ticks** with supplied intact targets. Both captures explicitly disable air-to-air acquisition and record scenery, radio and projectile requests without executing their downstream effects. Native allocation/SSD loading and ground-target lookup are declared host boundaries. Four compact, test-authored fixtures cover another **72,512 aircraft ticks**, including shared radio state, waiting/takeoff, extended formation movement, repeated attack bursts and cross-record profile reads. CTest replays these without original files. All 128 retail missions also run 512 integration steps and render their resulting scenes under ASan/UBSan.

The sweep exposed an original 2.05 data mismatch: aircraft groups in missions **721 and 923** (and five retained environment programs) reference the **DESTROYER surface profile**, at 30,000 feet and 500 knots. The native loader stores profiles contiguously, so its air controller reads into the following MISSBOAT record. The reconstruction retains a bounded copy of the relevant adjacent bytes and preserves native first-match clamp order, including inverted limits, without performing an out-of-bounds C++ read. This retains the reference executable’s observable behaviour.

The remaining air integration includes target acquisition, combat manoeuvres/CBN execution, actual projectile creation, incoming-threat responses, aircraft/world contacts and the complete damage/retirement lifecycle. The route comparison does not establish those behaviours or complete combat sessions. The live random stream continues from terrain variant selection, but full original-session random consumption is still a separate verification task.

## Air combat controllers and projectile motion

The live aircraft runtime now acquires opposing aircraft and the player, pursues, uses gun/missile release rules, turns away during its firing cooldown, and runs the original CBN manoeuvre programs. `CBN.DIR` supplies bank order; a resource's archive order is not substituted. Each aircraft owns its scan cursor, target identity, action stack, missile count, manoeuvre cursor and cooldown. Native acquisition scans six of fifty opposing slots at a time. F-117 player detection uses one eighth of the profile range and includes vertical separation; ordinary detection uses horizontal Chebyshev distance.

```sh
python3 tools/check_air.py --combat --ticks 1024
python3 tools/check_air.py --combat --synthetic --save-fixture tests/reference/air_combat.txt
python3 tools/check_projectiles.py --save-fixture tests/reference/projectile.txt
```

The combat comparison covers all **192 programs / 2,823,168 aircraft updates**, with a supplied stationary player near each program's first aircraft group. It checks every intermediate pose, controller state, scan/shot counter, target identity, CBN cursor, shared program byte and emitted request. Twelve independent test-authored scenarios add **98,304 updates**, including incoming threats, disabled weapons, stealth, CBN alignment/completion, countermeasures and firing cooldowns. The saved scenarios run through CTest without game assets or Python. Projectile effects and SSD calls are recorded boundaries in these comparisons; target damage/death is not simulated by them.

All 27 weapon definitions and their timed stage programs now load from the reference executable. Typed C++ projectile motion reproduces attachment/release, gravity, acceleration, guidance, range exhaustion, decoy diversion, low-altitude cruise and child dispensers. Native comparisons cover **108 retail sequences / 27,648 updates**, plus **96 test-authored sequences / 24,576 updates**. Inputs include timer intervals, target coordinates, mounted-store anchors and player ECM state; visual resets and child releases are captured. The original seam-crossing guidance error arithmetic is preserved rather than replaced by an idealised shortest-angle turn. The synthetic trajectory fixtures are asset-free.

Aircraft now react to the live player and launch real projectiles. The integration binds swept contacts to aircraft, surface actors, the player SSD and terrain; weapon effects use the original shared fired/secondary SSD instances. Aircraft whose SSD becomes destroyed enter the original fatal-hit controller. The player SSD can receive hits, but its reports are not yet joined to the complete player flight-damage/death machinery. Scoring, FINISH execution, player weapons/sensors and the ground-unit firing controller remain separate work. The runtime still needs the complete native startup/session random-consumption order and visibility-dependent SSD animation rules.

### Projectile populations, mutual damage and aircraft retirement

```sh
python3 tools/check_contact_damage.py --save-fixture tests/reference/contact.txt
python3 tools/check_projectile_runtime.py
python3 tools/check_air.py --combat --damage-tick 16 --ticks 1024
python3 tools/check_air.py --combat --synthetic --damage-tick 16 --save-fixture tests/reference/air_death.txt
```

The original pool has **98 projectile records**, plus two list sentinels. New shots go at the head and a full pool recycles its oldest record. The C++ pool retains stable record addresses and saves the next link before invoking motion, matching the original even when a dispenser recycles the record currently executing. Child construction runs synchronously; the stage program is re-read from the current record after possible reuse. Copying/moving the pool is disabled because its internal links depend on stable ownership.

The complete original population loop, constructors, removal and substep wrapper agree across **2,576 frames** at elapsed intervals 0, 1, 4, 10, 18 and 55. These cases include all 27 weapons, dispenser pressure at the original pool capacity, both countermeasure kinds, guidance transfer to moving decoys, decoy expiry, reset/registration order and contact-pass traversal. SSD rendering, actual world contacts, mounted-store anchors and aircraft threat registration are explicit boundaries in this population comparison. Contact arithmetic is checked separately; these are not complete mission traces.

The original contact callback reduces the querying projectile's health as well as the victim's custom health. Its signed-zero threshold differs from SSD scripts that branch only when custom health becomes negative. **2,048 test-authored mutual-contact updates** match the complete original VM/callback interaction, including zero contact cost (charged as one), negative thresholds and word wrap. CTest replays the saved asset-free cases.

Fatal aircraft transitions, ejection/falling, impacts and retirement match all **192 aircraft programs / 2,823,168 updates** after an externally supplied fatal hit. Twelve additional test-authored death scenarios add **98,304 updates** and run without game assets. These comparisons supply ordinary ground-impact effects and capture SSD/sound outputs; they do not establish scoring or collision causation. Aircraft/crashed-body visibility follows the original action state rather than waiting for every retired flag.

Native gun stages at `0xa422c` and `0xa42bc` have identical motion but different eligibility in the contact pass. They remain distinct typed stages. Weapon filenames also follow the original loader: the display label is separate, and flying SSD names are prefixed with `x` and truncated to eight characters.

AI request delivery is synchronous: a newly launched missile can become an incoming threat before the next aircraft is updated in the same tick. Chaff/flare diversion clears the previous threat association immediately and points guidance at the decoy's current position. Decoy expiry changes diverted missiles to the native coasting callback with its 30,000-tick lifetime. The live aircraft and projectile systems share one gameplay random stream; procedural vertex jitter remains separate.

All **128 retail missions** run 1,024 integrated four-tick steps and render under AddressSanitizer/UndefinedBehaviorSanitizer with live aircraft weapon releases and contacts. This is a robustness sweep, not a claim that every mission or end condition has been completed. Explosion/impact effect spawning, complete player death/results presentation, ground/designator targeting, flak/surface targeting and the mission-result bridge still need integration.

### Player cannon and mounting geometry

```sh
python3 tools/check_anchors.py --save-fixture tests/reference/anchor.txt
python3 tools/check_cannon.py --save-fixture tests/reference/cannon.txt
python3 tools/check_window_controls.py --data-dir /path/to/TFX
```

Space now follows the original keyboard cannon branch (`0x86898` → `0xa2fdc`), with the F-117 exclusion, landed/death/inhibition checks, the original FLP/COM availability checks, finite ammunition and alternating muzzle. The original starts at **1,760 rounds** and debits ten per visible projectile, including when fewer than ten remain. An unavailable FLP slot consumes a gameplay random value even without a shot; failed/intermittent nonzero bytes retain the native branch behaviour. Host keyboard polling drives this branch once per accepted flight frame, after player physics and before the other actors update; separate mouse-trigger acquisition remains unfinished. The Space-edge carried-store path is now connected as described below.

Native keyboard comparisons cover **4,096 original-mount cases** and **1,024 cases with test-authored mounts**, including negative/low ammunition, both barrels, every eligibility flag, wrapped coordinates and random-state effects. Cannon audio is a declared output boundary; the absent optional external-device callback executes normally. The synthetic cases run through CTest without game files. An isolated virtual X11 check additionally verifies actual Space input changes the live ammunition HUD and Escape exits.

Cannon and store mounts load from the original executable for all three aircraft. The complete native anchor transform agrees in **4,096 cases** (512 saved asset-free cases). Each signed high-word product is rounded separately before wrapped accumulation; replacing it with a conventional matrix dot product changes the result. Mounted-store attachment in live projectile motion now uses this geometry.

The original projectile constructor only updates its retained mount byte when the release angles match the current player angles. Untargeted releases also share one mutable aim point; every subsequent such release can change it. The population comparison now includes both details and totals **3,220 frames** at the native 98-slot capacity. Native substep elapsed values are used during child births; outside the update, releases see the caller's current elapsed word.

### Carried armament and air-to-air acquisition

```sh
python3 tools/check_loadout.py --save-fixture tests/reference/loadout.txt
python3 tools/check_sensors.py --save-fixture tests/reference/sensors.txt
```

Default loadout pairs, aircraft-specific mounting anchors and the pylon successor ring come from the selected original executable. The two native selection routines have different purposes: `0x8b9c4` skips another pylon of the current weapon type when cycling manually; `0x8bba0` advances to another usable store after release. The cannon is a separate sentinel in this ring, skipped by the F-117. The release primitive (`0x8bd8c`) retains byte-count wrap, the JP233 empty-pylon visual exception and Arcade/simulator unlimited-store configuration. Projectile construction runs before consumption/selection, so attachment observes the original launch pylon.

**6,192 original-code comparisons** cover all 30 defined retail fits, 18 test-authored fits, both selectors, empty slots, all 27 weapons, inhibition/unlimited states, wrapped coordinates, mount transforms and target-clear side effects. Empty slots use byte 255; the original selector reads two metadata fields beyond its 27-record table even for these slots. The C++ reader owns those two bytes explicitly rather than indexing outside the weapon vector. The synthetic cases are replayed by CTest without game assets.

Radar acquisition (`0x836ac`, `0x8296c`, `0x828b0`) preserves 20 contact slots across scans and fills holes with new contacts in original pool/enumeration order. Its 50-mile cutoff uses the original integer division and word operations. The air-to-air seeker (`0x93328`) preserves rear-aspect/cone/range/side tests, separate selected/tracked/guidance contacts, moving Q8 screen reticle and lock timers. Angular differences deliberately retain the original wide signed subtraction at the angle seam.

**1,024 persistent radar frames and 4,096 acquisition/lock states** agree with original execution. Screen projection and sound/radio output are supplied boundaries in this comparison; it does not prove the graphics projection or the separate laser, anti-radiation, cruise and anti-ship controllers. Native contact-buffer overflow beyond the supported 20 slots is not reproduced as memory corruption.

The live build connects Enter, Backspace/O, F, C and Space to selection, target cycling, cannon and finite carried-store release. The private X11 smoke test verifies both the store count and cannon ammunition change through actual keyboard input. All 128 retail missions initialise their default loadout, run 1,024 four-tick integration steps and render under AddressSanitizer/UndefinedBehaviorSanitizer. Complete cockpit radar drawing, ground/designator acquisition, separate mouse-trigger handling and missile-lock audio remain outstanding.

### Player system damage and disabled flight

```sh
python3 tools/check_damage.py --save-fixture tests/reference/damage.txt
python3 tools/check_disabled.py --save-fixture tests/reference/disabled.txt
```

The SSD percentage-damage opcode delivers its callback synchronously, before testing the fatal branch. This matters when the callback changes that SSD's state or animation. Player reports now execute entry 13 and the native system-damage calculation (`0x8f460`/`0x8f3ec`) on the same gameplay generator used by aircraft and projectiles. Solid contacts also debit the querying player's health and deliver the native 100-percent report. A regression exercises a re-entrant restoration callback before the fatal decision.

The native calculation samples eleven system slots in its authored order, even for repeated/zero reports. It distinguishes intermittent faults (14), failed systems (11), normal/active states and invulnerability. Intermittent faults clear strictly after 1,080 ticks in `0x871d4`; the word repair clock retains wrap/signed comparisons. **8,192 cases** agree in all system bytes, clocks, random state, stored damage double and notification count. The later cockpit work below establishes the slot names from the actual HUD label table and manual.

A lost player now enters the translated disabled/ejected flight branch (`0x8e5a8`) rather than staying flyable or freezing in mid-air. It retains automatic levelling, the special Mach 0.5 request, roll, matrix normalisation cadence, descent, supplied ground contacts and integer/float handoff. **4,608 complete callback steps** match original execution across the three aircraft, beginning with 64 ordinary flight frames before entering the disabled state. These sequence cases run through CTest without original assets. This establishes the motion branch, not the complete ejection camera, crash-effects lifetime or front-end result sequence; those remain to be joined.

Weapon-system faults participate in Space-edge store release, while the separate held-key cannon branch retains its original nonzero-byte checks. Engine display states are refreshed from the engine fields at the original frame boundary. The Arcade model's original handling is retained; physical-model damage effects and complete cockpit fault displays are still outstanding.

After joining these paths, all 128 retail missions again run 1,024 integrated four-tick steps under AddressSanitizer/UndefinedBehaviorSanitizer. All 24 CTests pass in normal, sanitizer and Release builds; private-window weapon input also passes. These are bounded subsystem/robustness checks, not completed-mission or end-to-end campaign proofs.

### Live mission outcomes and actor surfaces

```sh
python3 tools/check_progress.py --save-fixture tests/reference/progress.txt
python3 tools/check_credit.py --save-fixture tests/reference/credit.txt
python3 tools/check_death.py --save-fixture tests/reference/death.txt
python3 tools/check_world_contacts.py --save-fixture tests/reference/world_contacts.txt
```

The live session now drives authored target destruction/credit, all six completion kinds, protection failure, per-target timers, return-to-base scoring and delayed termination. Airborne starts use the native completion delay; landed starts require stopping after return. Timer comparisons preserve their strict boundaries, signed/unsigned distinctions and word wrap. The return-to-base award belongs to the parser's score variable, separately from the combat score passed to FINISH. **4,439 original-code frames across 512 supplied-outcome sequences** verify this rule/termination layer.

Aircraft and mobile losses now supply native type-filtered mission counts and combat credit. The player's kill acknowledgement consumes a random value before the fatal-aircraft routine chooses ejection/falling. Native quirks remain: some mobile/air credit is independent of the shot owner. **4,096 controlled native credit cases** cover ownership, permission, type filters, signed score wrap and violations. `setidnum` and `setmobileid` are named aircraft/surface type filters in C++, following their actual consumers rather than assuming the first only concerns static scenery.

Player loss and impact remain separate stages. **4,096 native transition cases** verify outcome/status codes and the strict 200-tick post-impact countdown, including repeated loss and invulnerability. The live FINISH handoff occurs once per terminated flight, using fresh direct-entry pilot statistics. A simple result display shows the resulting score/mission value until the original front-end graph and pilot persistence are connected. This is not a replacement campaign UI or a proof of a completed normal mission.

Player surface queries now include nearby mobile SSD assemblies, including carrier decks, after the terrain query. The native broad-phase box has a minimum radius of 1,024 world units and excludes its exact edges; **8,192 original region/eligibility cases** match, including word/dword wrap. Without this bridge, carrier starts lost their deck contact and eventually crashed while parked. These starts now remain on their actual deck. Surface flags use the native byte representation, distinct from aircraft word flags; hostile/dead/retired handling now follows that byte.

Compiled type-17 fixed placements also load, preserve authored initial events and traverse in the native reverse order. None is present in the 192 audited reference programs; **1,408 test-authored placements** verify the retained loader/list path against original execution. Surface heights remain the authored absolute word, rather than adding the actor's visual Y translation.

The private X11 input check additionally reaches a result through the live mission 605 combat path, verifies the result image stays stable after termination and closes it with Enter. Ground/designator targeting, complete permissions/court-martial handling, water/aftermath crash effects, moving-deck velocity transfer, the refuelling-aircraft collision path and original front-end result artwork remain outstanding. Direct launches currently use Arcade scoring eligibility; persistent game-mode configuration belongs to the pending front end.

All 28 CTests pass in normal, Release and sanitizer builds. All 128 retail missions again run 1,024 integrated four-tick steps under ASan/UBSan; the sweep checks that any reached FINISH handoff occurs exactly once. Mission 605 reaches its loss/result path naturally in this bounded run. Carrier-start mission 504 remains parked after the actor-surface fix. Full original/C++ mission traces remain a separate verification gate.

### Ground designation and weapon targeting

```sh
python3 tools/check_ground_sensors.py --save-fixture tests/reference/ground_sensors.txt
python3 tools/check_damage_centres.py
python3 tools/check_projectile_runtime.py
python3 tools/check_window_controls.py --data-dir /path/to/TFX
```

The live ground view uses the original unlocked gimbal/intersection (`0x92c24`, `0x93040`) and locked following camera (`0x92a34`). The F-117's relative gimbal limits, signed-word intersection radius, frame-based keyboard steps, ten zoom levels and different search/follow camera strides remain distinct. Z enters the designator and designates a point; arrows aim, comma/period zoom, X clears acquisition and F1 returns to the aircraft view. This is the retail 2.05 Z binding; the earlier manual's Y description differs. Display overlays/projection are still diagnostic rather than a complete original HUD.

Anti-radiation C selection (`0x7c828`) cycles live radar-emitter types and captures their current position. Harpoon's first Space press selects the nearest eligible ship (`0x7cb5c`, `0x9323c`); the next releases it with a live actor reference. Ship-type eligibility loads from the selected original executable. Laser-guided stores use the shared mutable designation point, rather than freezing a separate copy for every missile. Selecting a later point can therefore change guidance for an earlier release, as in the original.

Maverick selection (`0x7ca24`) searches eligible mobile damage zones near the designated point, preserving slot/zone ordering, strict range/tie comparisons and native wrapped squared distances. Its tracking point deliberately ignores actor heading and anchor height (`0x7c998`). The zone centres come from leading damage-once/staged boxes in SSD collision entry 2 (`0xb7b90`), separate from the visual model anchors. All **703 original SSD definitions / 698 targetable boxes** agree with native enumeration at the moving-instance orientation zero.

Original-code comparisons cover **8,192 unlocked designator/intersection states, 2,048 locked camera states, 4,096 wrapped distance estimates and 4,096 surface selections**. Mobile tests supply separately verified SSD centres; camera cases stop at the graphics boundary. A compact one-in-eight subset of synthetic inputs is replayed by CTest without game data. Projectile population checks now cover **3,864 frames**, including a moving shared designation, constructors, dispenser births, recycling and decoys. The private X11 check additionally selects a ground weapon, opens/zooms/aims/locks the designator and releases a store through real keyboard input.

Complete ground HUD/MFD artwork, target-planner interaction, cruise target programming, separate mouse-trigger handling and end-to-end native targeting-session traces remain outstanding. The current aircraft-view projection is still a supplied boundary in subsystem comparisons; matching acquisition arithmetic is not a claim of pixel-identical HUD output.

### Surface-unit combat

```sh
python3 tools/check_surface_combat.py --save-fixture tests/reference/surface_combat.txt
```

Mobile surface units now execute their nearby-aircraft firing requests (`0x7be44`) and authored ground-attack requests (`0x7bee8`) through the live projectile system. Enemy/neutral units consider the player first, then scan the allied aircraft pool backwards; allied units scan the enemy pool. The original retains different player/nonplayer range boundaries, F-117 detection reduction, per-unit player shot count, one-in-sixty random firing check, minimum launch speed and vertical anti-air launch angle. Ground attacks use their separate authored launch pitch and only fire at an existing intact terrain assembly.

Surface route requests now deliver synchronously before movement. Returning requests and consuming them afterwards would launch from the next position. The native comparison includes **512 complete route sequences**, alongside isolated acquisition/ground-attack cases, for **4,096 scenarios / 135,492 updates / 1,759 weapon constructor requests**. Original enumerators, bearing calculations, route/controller/movement code and random calls execute; terrain presence/destruction and the projectile constructor are explicit boundaries. A compact subset of synthetic cases runs through CTest without game data. These checks establish surface-unit firing, not the separate mission-area flak controller or a completed ground-attack mission.

### Native presentation media

```sh
python3 tools/check_media.py
python3 tools/check_media.py --build build-san
```

The C++ media library now decodes original `war ` screen images, four-plane sprites and the word-bit compressed delta animations (`0xde8cc`) directly from external archive resources. It keeps indexed pixels and the original palette, validates each delta run within its VGA plane and can retain the preceding screen as a delta background. Stored animation blocks and the declared playback frame count remain separate: three original resources contain extra stored blocks, whose playback meaning must be handled by the presentation caller rather than guessed by the decoder.

All **116 indexed screens, 39 sprite resources and 40 animations / 1,606 compressed blocks** compare byte for byte with independent extraction. Every animation block also executes in the original x86 decoder before final composed pixels and written coverage are compared. The complete corpus passes with ASan/UBSan, alongside synthetic literal-refill, retained-background and malformed-stream tests. No game artwork or original instructions are bundled. This establishes the media layer; original presentation scheduling, fades, fonts, menus and campaign dispatch are still being connected.

### Presentation graph decoding and control flow

```sh
python3 tools/check_presentations.py
```

All nine retail saved parser graphs now decode into typed clip/text, branch, assignment, audio, planner, menu and launch records. Editor linked-list/pointer bytes are discarded; destination ordinals and resource strings retain their actual data meaning. CP437 text is bounded by its saved length, which excludes a terminator in the retail files. Unsupported retained node families are preserved as opaque data rather than interpreted as a guessed action.

Every decoded semantic field, including offsets, text settings and native audio modes, matches independent extraction. The original numeric handlers agree for all **254 retail branch records and 163 assignments / 991 supplied-state cases**, including native gosub/return state and 32-bit wrap. This is the retail subset of the research corpus; the demo's additional 17-byte text layout is not silently accepted as retail.

The C++ control-flow runtime executes assignments, conditional/unconditional branches and subroutine returns until an explicit media/menu/session action is ready for its host. Synthetic tests cover wrapping arithmetic, return to the statement after a call, suspension/resumption and every truncated prefix of a test graph. Reconstruction safety limits reject non-yielding control flow and excessive calls instead of reproducing out-of-bounds memory access. This is not yet a claim of complete original front-end, pilot persistence or campaign-session execution; those host consumers remain in progress.

### Original presentation fonts

```sh
python3 tools/check_fonts.py
python3 tools/check_fonts.py --build build-san
```

All ten presentation fonts now load their original bitmap tables from external `TFX.DAT`. The two fixed fonts use code-object lookup rows; proportional fonts use their width, glyph-pointer and bitmap tables. No captured glyph images are bundled. The immutable executable asset tables own the decoded bank for presentation consumers.

**1,104 glyphs and 132 complete-string cases** match fresh original VGA execution in every pixel, written coverage, native width word and line height. Cases include black ink, all horizontal phases of the fixed-font string renderer, all four clipped corners for proportional styles and empty-string metrics. Native measurement and drawing stay separate: case handling, final spacing and the small font's extended-glyph advance differ. The fixed eight-pixel renderer also narrows a following uppercase I, while its width calculation remains fixed. Null original title-font pointers remain explicitly unavailable rather than borrowing another font.

The comparisons render complete VGA frames rather than relying on cropped glyph snapshots; a glyph can draw beyond the width reported by its measurement helper. Asset-free tests exercise owned table data, black coverage/transparent holes, extended baselines, distinct measurement/drawing advances and fixed-font termination. This establishes character/string rendering; briefing substitution, full text layout/reveal and presentation playback are the next consumers.

### Complete briefing text and reveal

```sh
python3 tools/check_text.py
python3 tools/check_text_clock.py --save-fixture tests/reference/text_clock.txt
```

Text preparation (`0xb1714`, `0xb14dc`) now handles original line breaks, ASCII uppercasing, named substitutions, signed numeric formatting and `^briefNNN.txt` language-file replacement. Substitution occurs after literal uppercasing, so inserted pilot strings retain their own case. Line widths, left/right/centred positions, line heights, shadows and visible prefixes follow the native routines. A bare CR retains its original two-byte skip. All shipped captions and language files fit the original twenty 80-byte line buffers; oversized/cyclic custom input receives a bounded error instead of overwriting neighbouring state.

**435 complete text cases and 123 revealed-prefix cases** match original preparation and VGA composition in every line, coordinate, count, pixel and written-coverage bit. These include **275 external references across all five languages**. The native file-loading boundary receives actual independently extracted language bytes; C++ reads those same assets directly from the archive. Static and partial drawing execute unchanged in the original, including whole-line shadow-before-foreground ordering.

Reveal timing (`0xb1b28`, `0xb1268`, `0xb1c16`) retains the fast counter's signed low-word delta, per-frame increment clamp of 1–50, two ticks per character, fractional remainder and Space consumption. **2,048 sequences / 131,072 native transitions** agree, including low-word/full counter wrap and completed/empty captions. Compact synthetic cases are replayed by CTest without game data. The input counter and the following VGA draw are explicit boundaries in that arithmetic comparison; the separate prefix comparisons establish the resulting drawn text. Full scene wait/fade/animation scheduling remains the next integration step.

All 35 CTests pass in normal, Release and sanitizer builds. The complete multilingual/prefix comparison also passes with ASan/UBSan. Font and text checks share a small declared planar VGA write-capture helper; it does not emulate the rest of the original game or hardware.

### Presentation timing and fades

```sh
python3 tools/check_presentation_timing.py --save-fixture tests/reference/presentation_timing.txt
```

Scene input (`0xb1128`), wait countdowns, animation playback (`0xc9fac`) and six-bit DAC fades now have native comparisons: **4,096 input cases, 4,096 countdown decisions, 65,536 animation updates and 240 complete palette uploads**. Compact synthetic cases run through CTest without game assets. Images hold their wait timer while text is revealing; animations do not, and text-only waits test the previous count rather than the decremented value. Space finishes an incomplete caption before advancing; mouse-button priority, Escape sequence skipping and Shift+Q retain their native ordering.

Animation clocks advance at most one block per callback and discard accumulated backlog. The initial frame plays once; repetitions resume at the second stored block over the retained final pixels. Declared frame count controls playback even when additional blocks are stored. Palette ramps scale the original six-bit DAC entries before display expansion, preserving integer rounding. These verified kernels are being connected to the native scene host; complete front-end execution remains outstanding.

### Integrated scene playback

```sh
python3 tools/check_scene_player.py
./build-san/player_probe --corpus /path/to/TFX
python3 tools/check_presentation_window.py --data-dir /path/to/TFX
./build/tfx --data-dir /path/to/TFX --presentation cdintro
```

The scene player joins media, text, input, countdowns and DAC ramps. The graph session retains nested-parser return positions, numeric state, sequence skipping and per-node key/reveal resets. Text-only scenes retain their pre-fade reveal clock; image/movie captions begin after their fade. Static captions compose before fade-in, and animation repetitions retain the previous pixels while resuming from block two. All ten font styles remain indexed software drawing. The retained text-only palette selector distinguishes the embedded palette from the caller's logical palette; fade uploads remain separate from logical palette storage.

**193 complete original still/text handlers match across 1,157 composed content frames**, including caption preparation/reveal, Space consumption, Escape termination and the surrounding scene loops. Asset loading, supplied timer samples, display flips/retraces and palette output are declared boundaries; decoding, fades and animation clocks have the separate complete comparisons above. The test reproduces the original parser caller's per-node reveal reset rather than carrying the previous caption count into an isolated handler. This does not establish a complete original front-end session or its inherited palette/clock state.

A sanitizer run completes **592 scene/language cases and 194,404 content/ramp iterations**, covering every one of the 372 retail visual nodes and all five languages for external captions. Those full playback runs establish resource and lifetime coverage, separately from native output agreement. Private X11 tests verify that Space advances one scene while Escape and Shift+Q finish the presentation cleanly. All 37 CTests pass in development, Release and ASan/UBSan builds.

The developer `--presentation NAME[:FUNCTION]` entry runs original graph scenes in the native window; `--frames N --screenshot FILE` provides deterministic headless captures at virtual 70 Hz. Audio actions are explicitly deferred. Menus, pilot persistence, waypoint planning and game launches remain host boundaries; reaching an unfinished host action reports it rather than fabricating campaign progress.

### Original options menus and pointers

```sh
python3 tools/check_menus.py
python3 tools/check_cursors.py
python3 tools/check_presentation_window.py --data-dir /path/to/TFX
./build/tfx --data-dir /path/to/TFX --menu mastopts
```

All eight shipped `.opt` scripts now load directly from the selected archive. Typed choices retain authored click bounds and declaration order, label destinations and numeric assignments. The supported script subset also retains bitmap/animation selection, cursor style and the no-selection-box flag; unsupported custom commands fail explicitly. No retail menu is replaced by a newly authored list.

The original interpreter agrees on **eight menu setups, 43 complete label/assignment paths and 17,104 mouse positions**. Tokenisation, original box registration, label lookup, numeric writes/returns and hit testing execute unchanged; file loading, graphics/pointer output, selection-box drawing and the interactive display loop are declared boundaries. CTest adds asset-free checks of overlapping inclusive bounds, signed dword writes and invalid labels.

Both embedded cursor styles and their active/idle sprites load from `TFX.DAT`. **144 pointer frames match original VGA sprite drawing**, including the four clipped corners, transparent holes and supplied positions. Right-edge clipping preserves the blitter's four-pixel group quirk. This checks pointer pixels independently from menu script semantics; it is not a complete native-menu frame comparison.

The menu host uses original indexed artwork, looping flag animation, palette fades and pointers. It applies a choice on press and waits for release before fading, matching the native `0xd5750` branch sequence. Asset-free tests cover held clicks and single assignment. A private X11 test clicks the FWS box through the corrected 4:3 viewport, checks that holding does not exit early, then releases and verifies the chosen index and clean exit. Pointer coordinates share the same fitted rectangle as final display, including framebuffer/window scale differences.

Options nodes now suspend/resume the graph session through this host. Direct developer `--menu NAME` launches allow checking individual scripts before pilot and mission dispatch are complete. Full original pilot/save/load/planner flow remains outstanding; this work does not fabricate those state transitions.

### Native pilot-save storage

```sh
python3 tools/check_saves.py
```

The save codec follows original writers/readers `0xb3aa8` and `0xb3c14`. Files contain **thirty fixed-order variable slots**, including three reserved numeric slots beyond the 27 declarations in retail `tfx.par`. Text occupies 21 bytes; numeric slots store complete little-endian dwords. A word return-stack depth precedes function/statement word pairs and the final resume pair. Return entries name the original call statements; native return processing advances past those sites. A function word of `0xffff` means there is no saved resume position.

**512 original write/read states agree** across all four writer modes: no resume, previous statement, current statement and following statement. Cases include reserved-slot bits, signed numeric bit patterns, empty/maximal strings and up to five saved calls. All four existing DOS saves in the reference install decode and re-encode with only bytes after string terminators cleared. No installation file is modified by the checks. New files initialise that padding instead of copying DOS's stale memory.

Asset-free tests cover complete round trips, every truncated prefix, trailing bytes, string limits and invalid saved graph positions. Loading rejects a stack beyond the original 100-entry storage and out-of-program positions rather than following arbitrary memory. The graph runtime can now snapshot all four native resume modes and restore saved call sites, values and reserved slots. A nested suspended-action test serializes, reloads and returns to the statement after the original call; the host explicitly completes the load action after restoration. This establishes file compatibility, serialization and runtime restore; pilot-selection UI, autosave timing and complete campaign-session wiring remain separate integration work.


### Pilot enrolment and front-end flight dispatch

```sh
python3 tools/check_pilot_names.py
./build/pilot_probe /path/to/TFX
python3 tools/check_frontend_window.py --data-dir /path/to/TFX
```

Normal startup now follows `tfx.par` through the introduction, enrolment and original main menu. `--skip-intro` enters the authored enrolment statement, preserving subsequent assignments. The front-end owns shared pilot variables and its suspended parser/menu; the existing flight loop is a callable host using the same window. Numeric launches and `%mission` launch the selected mission. On a completed flight, FINISH variables return to the parser; accumulated pilot score is preserved. Direct `--mission` entry remains available. Escape during an unfinished flight still exits the integration build rather than inventing a campaign outcome.

Pilot artwork, embedded arrow, labels, rank/status names and scan-code character table load from the selected original installation. Name editor `0xa1ee0` agrees for **1,155 complete native input sequences**, covering all make codes, eleven-character editing, Backspace, Escape/default names and Control qualification. The final callsign confirmation supplies the Control qualification bit; the original `WAYTXT/A` marker also qualifies new pilots. Edited names remain uppercase, while the twelve-character default surname remains intact. Pilot rows retain original status/rank/name ordering and the inactive-slot replacement/active-pilot overwrite prompts. The inactive-pilot “delete” confirmation marks only the chooser's in-memory entry empty until replacement, as the original does.

Enrolment saves, `slog`, `rlog`, `spos` and completed-training storage now connect to the native save codec. Existing lower-case DOS filenames are reused instead of creating case-only duplicates. Loading restores the saved parser statement and call stack before completing the enrolment/load host action. The new isolated test checks creation, qualification, persistence, load/replacement, native resume positions, completed-training markers and authored 501-to-502 training progression with a **supplied flight-result boundary**. That supplied result is a parser integration check, not proof of completing those engagements through combat.

The private X11 check uses a temporary installation containing links only to `DID.DAT` and `TFX.DAT`. It types names, qualifies/saves a pilot, chooses training, skips its briefings, enters live mission 501, exits, then reloads the pilot and enters again. It never modifies the reference installation or sends desktop input. Existing private flight weapon/targeting/results checks still pass. Development and Release builds pass all 39 CTests; the pilot integration check also passes in Release and ASan/UBSan. Full original enrolment-frame comparison, planner/loadout, high-score consumers and complete flight/campaign-session equivalence remain separate work.


### Original high scores and Arcade return

```sh
python3 tools/check_scores.py
python3 tools/check_score_frames.py
```

The original high-score roster, signed ordering, display and `tfx.hs` storage now run through the front-end host. `sscore` displays the table; `vscore` also inserts the current pilot's score, including the authored Arcade menu's view-scores path. Strictly greater scores qualify, so tied entries retain their existing precedence. Missing files use the embedded defaults; failed checksums restore and rewrite them, matching the original loader. Names come from executable tables and current pilot variables; no default roster or captured screen is bundled.

The 670-byte payload contains ten 67-byte records: three 21-byte names and a signed score. Native `0xa69b0`/`0xa6a10` use a privately reset original random sequence for byte XOR and the following checksum contribution. **512 complete encoding/checksum/decoding cases, 512 corrupted files and 1,024 native insertion cases agree**, including signed extremes and ties. New string padding is zeroed rather than copying old buffer contents. Asset-free tests check every corrupted byte, every truncated prefix, trailing data, name bounds and stable ranking.

The complete original high-score handler `0xa6c3c` agrees across **274 VGA frames**: the expanding text clip, exact labels/numbers, font pixels, shadows and all thirty selected-row highlight colours. Background decoding, display pages/retraces, cached palette and final mouse exit are explicit supplied boundaries. Native formatting, clip setup, fonts and screen-loop control run together. The right clip edge is exclusive; the bottom edge is inclusive. The display and highlight retain the original retrace-driven timing.

The isolated pilot/front-end test additionally chooses Arcade, writes its temporary log without a resume position, submits/displays a score, dismisses the table, returns through `rlog` to the main menu and checks repair of a corrupt score file. All files remain in a private temporary directory. Development, Release and ASan/UBSan builds pass 40 CTests and the extended pilot integration test. Planner/loadout and complete campaign-flight-session equivalence remain outstanding.


### Editable planner documents

```sh
python3 tools/check_planning_documents.py
python3 tools/check_planning_documents.py --build build-san
```

The planner's text `.WAY` resources now load into owned route/waypoint structures with retained commands, map/world/palette metadata, mission parameters and original briefing bytes. These are separate from the compiled `.WAY` programs used in flight. The data layer does not yet replace the pending planner host action or provide its interactive map/loadout screen.

Native `0xa7dc0` executes its complete line reader, scanning, path construction and command constructors with a bounded supplied allocator. **180 original loads agree across 8,916 waypoint records**, including all **30 retail Tour of Duty planner sources**. The compared fields are all sixteen resulting parameter words, map/world/palette names, route labels, waypoint ordinals/coordinates and the exact remaining briefing byte range. Source commands are preserved verbatim by C++; their planner compilation/editing semantics are a separate consumer, not claimed by this comparison.

The original header uses fifteen dword `%d` destinations spaced two bytes apart. Following fields overwrite preceding high halves, while a partial header leaves its final high-half write in the next word. The typed parameter loader preserves this effect, including legacy headers that provide only a format tag and retain initial defaults. Fields whose consumers are still unresolved remain explicitly retained. Native coordinate scanning also narrows to signed words; C++ does so without overflowing a waypoint allocation.

Two retained version-1000 Somalia sources, `somal_n.way` and `somal_dc.way`, omit their first route name. They are outside the thirty retail planner actions. The 2.05 reader misinterprets the following lines; `somal_n` reaches a null command-pointer write. C++ rejects these malformed route headers explicitly. The comparison checks those rejections separately from the 180 matching valid loads. Asset-free tests cover preserved commands/briefing bytes, native header overlap/spill, omitted defaults, signed coordinate narrowing and all truncated prefixes before the final route terminator. All 41 CTests pass in development, Release and ASan/UBSan; the full document comparison also passes with the sanitizer-built loader.


### Compiled planner artwork and palettes

```sh
python3 tools/check_planner_panel.py
python3 tools/check_planner_panel.py --build build-san
```

The original planner's frame is an executable-backed painting stream (`0xa9c34`), rather than a separate bitmap. A bounded decoder reads that fixed 2.05 artwork representation from external `TFX.DAT` and produces indexed pixels with explicit written coverage. It accepts only the painter's ink/run, position, repeat and VGA-plane-mask operations; it does not execute original game code or introduce a CPU emulator into the game. No captured image or original painting program is bundled. The coverage mask keeps the map rectangle transparent and distinguishes untouched pixels from written black.

The complete original painter agrees in **all 64,000 pixels and coverage bits**, including the curved frame details and plane-specific writes. The planner palette also agrees for **128 complete native compositions** through `0xab3ac`: map colours remain underneath the 80-entry interface ramp and sixteen marker colours. The interface bank lives beside the painter in the code object; the marker bank is in the data object. C++ reads both original banks as data. The comparison also passes with ASan/UBSan, and the Release/sanitizer planner tests check overlay composition without erasing uncovered map pixels.

These assets are ready for the interactive planner consumer; loading a planner action remains an unfinished host boundary until its route briefing, editing and arming workflow are connected.

### Planner map, annotations and route artwork

```sh
python3 tools/check_widget_font.py
python3 tools/check_planner_maps.py
python3 tools/check_planner_notes.py
python3 tools/check_planner_routes.py
```

The planner now owns its three original map layers, interface palette banks and region labels. Native order is physical relief, AWAC and political. The proportional widget font is separate from the presentation and HUD fonts and loads from the original executable. **416 complete native font cases agree** in width, pixels and written coverage, including every byte code, accents, plane phases, top clamping and the bottom edge. Immutable executable assets are decoded once and shared by their consumers.

**90 complete map pages agree** in every pixel and DAC entry across all thirty retail planner actions and all three map layers. **174 complete annotation windows agree pixel for pixel**, including their original frame, positioning, text limit and text/frame overlap. **177 route pages agree** at every waypoint in the retail briefing reviews, including native line order, numeric labels, edge placement and clipping. These comparisons execute original loaders and complete painters with supplied DOS resource access, bounded heap storage and four separately captured VGA pages. Optional satellite/world 3D setup remains outside these 2D comparisons. No captured artwork or game instructions are bundled with the C++ runtime.

Annotation indexing preserves the original discrepancy between its two number scanners. Blank lines after ESC in mission 829 cause notes 4 and 6 to replace key zero, while their widget headers still load correctly. Text is bounded to the original 400-byte buffer. Unselected native route labels read a header before the route array; the reference supplies zeroed mapped heap guard storage, while C++ expresses the unselected condition without that out-of-array access. Asset-free regressions cover annotation defaults, duplicate replacement, carriage returns and text bounds.

The map/annotation/route painters are currently independent components; interactive planner state, route editing, loadout and flight handoff remain the next consumers rather than being implied by these frame comparisons.

All four new native comparisons pass with development, Release and ASan/UBSan probes. All 41 CTests also pass in those three configurations.

### Live Tour of Duty review and translated notes

```sh
python3 tools/check_planner_segments.py
python3 tools/check_planner_review.py
python3 tools/check_planner_languages.py
./build/pilot_probe /path/to/TFX
python3 tools/check_frontend_window.py --data-dir /path/to/TFX
```

The front-end now handles all thirty retail planner nodes. Their mode is **1**, which calls native `0xaceb4` to review the player route, queues action 2 and returns automatically to the presentation parser. This is distinct from the editable route/mission-authoring modes. There is no invented Enter-to-launch step: Space or held right mouse dismisses each available note, route legs reveal in order, and the authored graph then launches the mission.

The animated line painter uses signed 16.16 DDA sampling and excludes its final endpoint, unlike the completed route's inclusive Bresenham lines. **727 complete native segment captures agree** in every pixel and returned remaining length, including zero, partial, complete and excessive extents. **Thirty complete original reviews agree in 2,004 note/reveal transitions and successful automatic return**. The original controller, note waits, geographic label loading and segment completion logic execute; optional world/satellite setup remains a supplied boundary. The original forty-retrace fade executes too: **1,230 complete displayed palettes agree**, including each ramp stage and the final black screen. These event comparisons complement the separate complete-frame painter comparisons; they do not claim an exact desktop-time/display-flip recording of an entire review.

Translated `.way` files overlay notes by key rather than replacing all English waypoint instructions. **150 live introductory screens match the original pixel for pixel**, covering every retail review in English, French, German, Italian and Spanish. The check also verifies that Enter/Escape do not dismiss the C++ note and Space does. Notes retain their raw blocks until requested, matching native delayed widget decoding. This matters for French mission 829: a stray `3` block is replaced by a later note before it can be displayed. Eager decoding would reject valid gameplay. Asset-free tests cover this replacement and translated overlays retaining other keys.

Pilot integration now follows the original main menu, Tour squadron choice, mission 801's review, its flight-launch boundary, a supplied successful result, and mission 802's review/launch. The supplied result tests campaign continuation independently of combat. All thirty text player routes also agree with their compiled flight waypoint lists. Editable authoring modes and loadout selection remain separate work. The segment/review/language comparisons and pilot integration pass in development, Release and ASan/UBSan; all 41 CTests pass in each configuration. A private X11 test also enters live FWS and Tour flight through the actual Release window, including pilot reload and held-right-button review dismissal.

### Arming permissions and HUD glyphs

```sh
python3 tools/check_arming.py
python3 tools/check_hud_font.py
```

Original weapon records now expose all three aircraft's mounting masks. The typed inventory supports direct assignment/clearing separately from interactive permission checks, as native `0x8b3d4` does. Native `0x89c08` also makes slots 0/7 mutually exclusive with 3/4/8/9 in the F-117 and F-22. This checks model presence, including temporary placement previews, rather than remaining ammunition. Eurofighter uses its masks without those additional exclusions.

**11,624 complete native states agree** across original and synthetic mask tables, all occupied-slot pairs, unrelated stores, emptying a slot and direct assignments that an interactive permission check would reject. Eighty-four compact original oracle cases also run without game assets in CTest. Existing default-fit, pylon-cycling and release checks still agree across **6,192 cases**.

The existing HUD glyph painter agrees with native `0x87154` across **576 complete frames**, including every byte code, black/white ink, all horizontal plane phases, clipping and embedded NUL. The C++ painter now stops at NUL, matching original strings. This font also draws the arming screen's small labels. Native VGA capture permits writes to each page's unused physical padding while retaining only visible pixels; all ninety planner map-page comparisons still pass.

Both new comparisons and all 42 CTests pass in development, Release and ASan/UBSan. These changes provide inventory permissions and glyph rendering for the arming interface; its interactive controller and 3D preview remain in progress.

## Carried-store model attachments

`models::interpret` now executes model opcode 81 against a supplied, borrowed table of carried shapes. The disk slot operand indexes 16-bit handles, and each child starts at its first detail program with condition byte `0xf9`. It inherits the current transform and scale. Vertex storage remains shared: weapon programs deliberately use high vertex slots, and those writes survive the return. The parent translation and condition are restored. All nested models share the instruction and depth budgets. Emitted primitives retain their attachment slot for loadout selection.

```sh
python3 tools/check_attachments.py --build build --research ../tfx-reverse
```

Twenty original-handler executions verify present/absent handles, anchor installation, child condition and parent restoration. A separate comparison covers **1,452 complete preview loadouts**: all 22 selectable stores, each of ten mounts and all mounts together, on each of the three player aircraft and their three dedicated arming models. Every primitive and vertex agrees with the independent inspection interpreter. This latter comparison does not establish complete native camera projection or raster equivalence. Asset-free tests also exercise shared vertex storage, absolute child scale, absent stores and recursive malformed programs.

### Original arming controls, readouts and preview camera

```sh
python3 tools/check_airframes.py
python3 tools/check_arming_controls.py
python3 tools/check_arming_screen.py
python3 tools/check_orbit_camera.py
```

The external executable supplies arming labels, hit rectangles, aircraft preview names, slot pairs and label positions. Aircraft store mounts and fuel capacity are decoded from literal assignments in the original compiled initializers. The Eurofighter and F-117 base moments retain their original square-root and ordered multiplication calculations; all **99 double fields agree bit for bit** with the three native initializers.

The controller owns its temporary mount markers separately from actual inventory. Markers participate in the original F-117/F-22 exclusion rules while compatible slots are marked sequentially. Placing and clearing affect the paired slot too. Escape selects the mission's defaults only if the native model table is empty, including its temporary markers; placeholders are then removed on exit. Right mouse cancels placement. The original strict rectangle edges, pointer adjustment and last-match precedence are retained. **3,072 native placement/clearing/exit sequences and 7,241 native hit tests agree.** Input-release waits and 3D picking are explicitly outside these controlled calls.

**192 complete arming widget frames agree pixel for pixel**, covering all aircraft, all selectable stores, scrolling positions, empty and mixed loads, mass, drag and inertia. The original drag display uses the preceding frame's accumulator. Watcom silently drops a malformed trailing percent sign in its format string; this omission is preserved. The comparison supplies original background artwork at the image-driver boundary and excludes 3D preview drawing; original formatting, font painting, outlines and arithmetic execute.

The arming screen uses `efa_glob`, `f17_glob` and `f22_glob`, which have dedicated mount-selection collision boxes. Original startup issues event 5 after the fade-in; the latter two definitions use it to open their weapon bays. The preview camera uses native `0xdabd8`, whose shift/negation order differs from the ordinary component matrix. Native `0xb9b7a` also orbits with a one-table-step phase offset at zero. **4,121 complete view matrices and signed Q16 camera positions agree exactly.** Custom projection origins leave the existing flight projection's operation order unchanged.

The aircraft, control, widget and camera comparisons, plus all 43 CTests, pass in development, Release and ASan/UBSan. These are subsystem comparisons; the assembled 3D screen and its live flight handoff remain in progress.

### Live arming screen and flight handoff

Normal campaign flight now opens arming unless the mission explicitly requests default weapons. It uses the original dedicated aircraft, carried-store and empty-pylon models, including the small aircraft pointer and rotating weapon previews. Store selection creates compatible mounting markers; the aircraft's original SSD collision entry picks a mount and fills its paired mount. Right mouse ends placement. The chosen inventory passes into flight without being replaced by the default fit. Direct mission launches retain automatic default fitting for analytical runs.

```sh
python3 tools/check_arming_picks.py
./build/arming_preview_probe /path/to/TFX flow
python3 tools/check_frontend_window.py --data-dir /path/to/TFX
```

**12,860 original mount selections agree**, covering each aircraft's actual selection boxes, directed edge cases, swept pointer segments and the initial zero position. The original query transform and SSD event dispatcher execute. The native actor draw wrapper also agrees with **60 complete arming bay-animation states**: arming holds the animation-enable byte at one, so these streams advance per draw rather than using the flight world's seven-tick animation clock. The original opening event is captured directly from the complete arming startup before its input loop.

The view setter stores **rows**, used directly as right/down/forward. The model vertex multiplier produces Q15 values while actor and camera translations use Q16, so arming geometry uses half-sized model units. Native model projection shifts signed Q16 X/Y by eight and divides by signed integer depth, truncating towards zero. **4,096 complete vertex products and 4,096 projected vertices agree**, alongside the earlier 4,121 camera states. The existing flight/inspection camera retains its continuous projection and operation order.

The unavailable-store panel and mounted-store labels agree in **82 complete original frames**, in addition to the 192 normal widget frames. Their rectangles include both far edges. These native solid spans require the actual A0000 VGA aperture; the harness now optionally relocates the data object using the executable's LE records, leaving the original raster instructions unchanged. Existing planner/font captures retain their previous default memory arrangement. The assembled arming screen also agrees with **81 complete palette frames** covering the original forty-stage fade-in, forty-stage fade-out and final black.

Headless pointer flows select, place and retain the intended paired stores on all three aircraft. A private X11 test also selects AIM 9S, mounts it, ends placement and launches FWS flight with the original DONE button; it repeats after pilot reload and still covers Tour review/arming/flight. No user installation, saves or desktop input are modified.

These comparisons establish the controlled subsystem boundaries above. They do not yet establish a complete original 3D arming-frame match. Full modal quit handling and input-release timing remain separate work, as do the broader flight-renderer and loadout/aerodynamics integrations.

The assembled-screen comparisons, three pointer flows and all 43 CTests pass in development, Release and ASan/UBSan. The private custom-arming window flow passes in development and Release. The shared capture changes also preserve all ninety complete planner map pages and 576 HUD-font frames, and all 128 retail mission starts still render successfully.

### HUD sprite and embedded-mask painters

```sh
python3 tools/check_hud_images.py
```

Shared sprite painting now serves the menu pointers and cockpit instruments. The original four-pixel right-edge clipping, transparent sprite index zero, MSB-first monochrome rows and explicitly painted black pixels are retained. Embedded masks use rounded-up byte strides, including widths that are not multiples of eight.

**572 complete native pixel-and-coverage frames agree** across all frames in eighteen recovered HUD banks and thirty-two embedded mask addresses, including edge clipping and black ink. The 144 menu-pointer frames still agree after sharing the painter. These comparisons and all 43 CTests pass in development, Release and ASan/UBSan. This verifies the instrument drawing primitives; live HUD composition is the next integration step.

### Cockpit clock, fuel, throttle and countermeasures

```sh
python3 tools/check_readouts.py
```

Typed telemetry now feeds the original clock, fuel, throttle, ECM/stealth and chaff/flare readouts. Their labels, formats, warning thresholds and coloured-font mapping come from the external executable. The clock uses a wrapping 32-bit tick sum and 54 ticks per second. Throttle percentages truncate the extended product towards zero; low fuel switches from coloured sprite digits to the small HUD font. ECM takes precedence over stealth, and the F-117 omits the separate stealth label.

**504 complete native pixel/coverage/status frames agree**, including each warning threshold, fractional values, countermeasure counts, clock overflow, horizontal plane phases and the distinct bottom-edge clipping rules. Original Watcom formatting executes in the comparison, including floating fuel values. The existing 576 HUD-font frames still agree after adding optional coverage recording. These comparisons and all 43 CTests pass in development, Release and ASan/UBSan. The readouts are ready for the assembled cockpit; flight still uses its diagnostic overlay while that composition is implemented.

### Multifunction-display instruments

```sh
python3 tools/check_mfd.py
```

### Gravis sampled-effects request controller

Gravis selects a separate four-voice sampled-effects path, rather than another
OSL music driver. Original `0xdda88`/`0xddadc` validate the complete sequence
before changing a cooldown. Entirely resident sequences rotate through voices
1–3; any streamed item selects voice zero. The rotating cursor advances even
when that resident voice is busy and priority rejects the request. Priority is
shared across voices, and a background request requires a stopped voice.
Accepted requests stop and replace their selected queue, including sequences
whose samples all have zero length.

The translated controller agrees in **8,192 complete original variadic
requests and persistent cooldown updates**, including 2,507 accepted requests
across all four voices. Original validation, queue assembly and first-item
dispatch execute; stopped flags and sample residence are supplied at their
hardware/resource boundaries. Exact stop/start calls are checked. A 256-case
synthetic fixture adds the 199th asset-free regression, passing in development,
Release and ASan/UBSan. This verifies scheduling; live Gravis playback and
sample-RAM allocation are still being connected.

```sh
python3 tools/check_gus_scheduler.py
```

### Gravis sample-RAM planning

The loader's original pairwise size swaps are preserved, including indirect
reordering of equal-size samples. Zero-length entries are skipped. A failed
large allocation does not prevent later smaller samples from fitting. The
driver reserves 32 KiB for streaming; its command-5 unsigned comparison rejects
an exact fit, leaving at least one byte free after successful allocations.
Its zero-byte request can succeed without advancing the cursor. Native linked
indices use nine signed bits, with `-1` terminating the list.

The comparison executes the original TFX sort, load walk and resident-flag
updates, forwarding allocation calls to the original real-mode `GUS.COM`
interrupt dispatcher. RAM capacity and sample lengths are declared inputs;
filesystem copying and hardware upload are observed separately. **1,024
complete plans and 32,097 direct driver allocations agree**, across all four
RAM sizes, zero samples, ties and exact-fit boundaries. There are 128 compact
synthetic fixtures. All **200 regressions pass** in development, Release and
ASan/UBSan. Live Gravis playback remains separate work.

```sh
python3 tools/check_gus_memory.py
```

Seven original page painters now consume typed flight, fuel, damage and inventory telemetry: stores, gauges, flight data, four fuel cells, system warnings, aircraft schematic and engine load. Their backgrounds, monochrome masks, labels, positions, display mount ordering, formats and conversion factors come from the external assets. Selected stores retain the original readiness colours and aircraft-specific mount arrangement; failed systems retain their outlined cells. The fuel display preserves the signed-word comparisons inside its dword subtraction loop.

**588 complete native pixel/coverage frames agree**, covering both outer display positions, all three aircraft, damage states, fractional values, viewport clipping and fuel-cell boundaries. All **eighteen original outer-page selections** agree independently. The two dispatch tables differ: the right display adds the moving map and engine load, while the left supplies the aircraft schematic and ground database; gauges occupy different indices. Scene-dependent camera, ground database, moving-map and control/autopilot pages remain separate integration work.

The HUD width helper also agrees for all **576 font cases**. Native `0x87114` measures the full character byte instead of the painter's masked index, and its partial-register operation retains the upper word for negative mapping entries. Centred MFD labels retain the original signed low-word conversion. The comparison executes those original instructions rather than deriving expected widths from the C++ mapping.

The MFD, readout and font comparisons and all 43 CTests pass in development, Release and ASan/UBSan. These remain verified pieces of the forthcoming cockpit composition, rather than a claim of a complete flight frame.

### Compiled cockpit artwork

```sh
python3 tools/check_overlays.py
```

The bitmap compiler's restricted literal pixel/plane streams now decode into indexed artwork records. Canopy column-entry tables become bounded data with their original horizontal-byte and vertical-row offsets. The fixed missile/map overlays retain their fixed placement, even when offset arguments are supplied. Explicit black writes retain coverage separately from untouched pixels. No original x86 code executes in the game.

**68 complete native pixel/coverage frames agree** across `infcock.hud`, `cockpit.bin`, its `a.bin` alias, `misshud.hud` and `cockmap.bin`, including extreme legal horizontal pans and vertical shifts. Asset-free tests cover every truncated prefix, unsupported tokens, invalid plane masks, out-of-screen addresses, unused bytes and fixed-overlay argument handling. The native captures and all 44 CTests pass in development, Release and ASan/UBSan. Viewport selection, the companion lower-panel upload and flight composition remain separate integration work; decoding retained artwork does not establish that every resource is selected in retail flight.

### Warning and auxiliary indicators

```sh
python3 tools/check_warnings.py
python3 tools/check_auxiliary.py
```

The warning-lamp painter retains the original gear, wheel-brake, hook, autopilot, warning/caution, fire, lock/launch, stall and threat sprite selection. It consumes typed system conditions and explicit display flags. Its four-draw phase and threat-sound latch belong to the painter's persistent state. Clearing the threat does not reset that original one-shot sound latch. Original lamp artwork and code consumers establish these meanings; nearby flags are not assumed to describe afterburner or ground contact.

**1,024 complete native pixel/coverage/phase/sound frames agree**, including every system's failure/intermittent warning and tracked-weapon category branches. Another **512 complete native pixel/coverage/refresh-state frames agree** for engine lamps, approach cues, ground indicators and afterburner markers. The engine-cache bytes are read but not rewritten by this native region. Afterburner pips are paired 2×2 points, rather than line segments; their original linear VGA edge wrapping is preserved. The approach-cue ordinal remains explicit until its selecting flight logic is ported.

These comparisons, the existing MFD/readout comparisons, and all 44 CTests pass in development, Release and ASan/UBSan. Sound requests are compared as events; the mixer and complete HUD composition remain in progress.

### Per-viewport HUD clipping

```sh
python3 tools/check_clipping.py
```

The renderer now owns explicit glyph and sprite clipping state. Native `0xc0634` keeps the supplied glyph X limit, rounds sprite X limits to four-pixel VGA groups, and increments bottom Y for both. Instrument painters consume that state; their earlier controlled inspection-driver limits remain the default until a viewport is selected. The sprite painter also retains the original planar edge reads when both horizontal sides are clipped with unrounded initial limits, reconstructing them within owned image storage.

**1,601 complete native pixel/coverage frames agree**, covering eighteen original sprite banks, embedded masks, every character byte, interior display rectangles and directed/synthetic plane-boundary cases. The 572 image, 576 font/width, 588 MFD, 504 readout, 1,024 warning, 512 auxiliary and 144 menu-pointer comparisons still agree. These checks and all 44 CTests pass in development, Release and ASan/UBSan. This supplies the clipping transitions needed by the moving map and assembled HUD; it does not yet replace the diagnostic flight overlay.

### Scrolling cockpit map

```sh
python3 tools/check_moving_map.py
python3 tools/check_attitude.py --data-dir /path/to/TFX
```

The moving-map painter owns its schematic and blink state, borrowing typed Q8 aircraft/surface contacts and integer-world targets for each draw. It preserves separate coordinate division before subtraction, north-up scrolling, the heading arrow, strict marker borders, aircraft altitude filtering, stealth suppression, target/navigation colours and native clipping restoration. Elapsed ticks are added per map draw; even a large elapsed interval toggles the marker colours at most once in that call. A shared painter must therefore serve both map locations when both are visible.

**320 complete native pixel/coverage/blink/clip frames agree** across all five original regional schematics and the centre/right display rectangles. The native actor-enumeration boundaries receive supplied lists; map projection, filtering, sprite drawing and marker painting execute unchanged. The shared signed-word planar rotation also agrees in **5,120 native cases**, covering every lookup-table phase and extreme inputs; the existing direction, basis rotation, normalisation and angle checks remain unchanged and still agree.

These comparisons and all 44 CTests pass in development, Release and ASan/UBSan. Binding the environment schematic and live flight contacts into the assembled HUD remains part of cockpit integration.

### Horizontal and beyond-visual-range radar

```sh
python3 tools/check_radar.py
```

Both central radar views now use their original display sprites, range/divisor tables, signed-word rotation and authored BVR envelope. Supplied aircraft/surface/projectile records preserve native Q8 positions. The horizontal view retains role and stealth filtering, selected-aircraft blocks, blinking radiating defence sites (`defop1`–`defop3`, type 56), projectile ownership colours and excluded cannon/decoy records. The BVR painter has its own filtering rules and does not apply the horizontal painter's failed-radar early return. This is a statement about these drawing routines, not a complete damaged-flight session.

Intermittent-radar samples consume the shared gameplay generator in original draw order. Allied surface markers bypass that sample in the horizontal view, while BVR samples them. Projectile cross arms remain visible independently of the sampled blinking centre. The horizontal pulse changes even when a subsequent stealth/failure gate suppresses contacts; BVR does not advance that pulse.

**640 complete native pixel/coverage/pulse/random-state frames agree**, covering both modes, every range, all aircraft, system conditions, selected contacts, surface types, projectiles and viewport offsets. Native actor-enumeration and fixed definition-type lookups receive declared input records; original projection, filtering, formatting and VGA drawing execute. Shared one-pixel and 2×2 marker painting also preserves all 512 auxiliary frames. The radar, auxiliary, map and MFD comparisons and all 44 CTests pass in development, Release and ASan/UBSan.


### Controls and autopilot instruments

```sh
python3 tools/check_controls_display.py
```

The controls MFD now draws the original surface pointers, F-22-specific indicators, flight-configuration labels and all five autopilot modes. Its artwork, formats, display scales and conversion constants come from the executable. The separate autopilot indicator preserves the original mode number even when the autopilot is disengaged. This establishes the displays; the autopilot controller itself remains separate work.

**768 complete native pixel/coverage frames agree** across both MFD positions, all three aircraft, modes, configurations, deflections and instrument offsets. Another **256 frames agree** for the autopilot indicator, including signed mode-number boundaries. Native Watcom formatting and the original VGA line painter execute in the comparison. Pointer endpoints stay within the native scanline table, whose out-of-range reads are undefined. The earlier 588 MFD and 504 readout comparisons continue to agree.


Scene submissions now seed the model condition byte from SSD part parameter zero, following native `0xcc001` and `0xbac16`. Previously the scene adapter copied the parameter array but left this separate initial condition at zero, preventing state-selected geometry such as the water-crash effect from selecting its authored branch. The original queue-to-dispatch path independently agrees for all 256 byte values; this corrects scene context, not model decoding or full-frame renderer fidelity.


### Assembled instrument HUD

```sh
python3 tools/check_hud.py
```

The instrument compositor follows the complete original `0x803e8` draw order: range notice, readouts, left/centre/right MFDs, warning lamps, autopilot selection, then auxiliary indicators. Fuel status changes before the MFDs and warning lamps consume it, and is returned for the caller's system state. Both outer display dispatchers retain their previous page and request the original shared redraw on a change. Both moving maps share one blink clock; radar faults consume the same generator as gameplay. Aircraft and surface contacts, projectiles, targets and navigation are explicit borrowed inputs.

**504 complete native pixel/coverage/animation/clip/random-state frames agree**, exercising all three aircraft, every currently implemented outer page, all centre modes, warning thresholds, page transitions and contacts. The original instrument routine runs as a whole; archive access, actor enumeration, surface-type lookup and warning sound output are declared boundaries. The separate upper aiming HUD is explicitly disabled in this capture, and the scene-camera pages remain a separate rendering consumer. Flight integration is the next step.

The manual also identifies the control MFD's trailing/leading-edge flap pointers and F-22 thrust-vector indicators. Flight-state comparisons establish that the four lower-left pips show afterburner stages (`0x1496d`), so the earlier provisional ammunition name has been corrected without changing their painter.


### MFD scene-camera requests

```sh
python3 tools/check_scene_displays.py
```

The FLIR/DLIR and ground-target database now prepare the original integer camera positions and view angles. The F-117 retains its ±45-degree free-camera gimbal limits. Target bearings, two-draw reticle phase, the fixed-distance ground orbit, height clamps, heading increments, range labels and reticle masks follow their original handlers. The scene consumer uses the independently verified native view matrix and projection origin.

**2,304 native cases agree** in camera requests, persistent gimbal/orbit/reticle state and all overlay pixels, across both displays, all aircraft and three orbit-step seeds. Full world rendering and the projected reticle point are declared boundaries; this is not a complete 3D pixel comparison. The assembled 504-frame instrument HUD and 1,601 clipping comparisons continue to agree, as do all 44 CTests.

An original bug is explicit: `0x88378` reads a roll word after `0xc9714` writes only pitch and heading. It can therefore inherit previous stack contents (the capture exposed a prior ground-camera height). The comparison supplies this otherwise unwritten input deliberately. The new native host will supply zero rather than depend on uninitialised C++ memory; a future DOS trace can establish whether a stable retail value deserves reproduction. Reticle testing also corrected monochrome painters to sign-extend their word coordinates, including MOVZX arguments that wrap at the top or left edge.


The original MFD key callbacks are now represented by the selection state: 1/2/3 cycle their respective displays, R decreases/wraps the active radar range, the F-117 skips BVR, the left ground database is skipped without authored targets, and the right camera page has its original aircraft/weapon-category eligibility. Wrapping to the first page happens at draw time. **1,920 native cases agree**, including word boundaries and the signed normalization rules (`tools/check_display_controls.py`). Environment configurations now supply their authored schematic basename rather than inferring one from the terrain filename; palette, night flag and map bindings agree for **all 33 decoded native environments** (`tools/check_environment_bindings.py`).


### Live instrument integration

The flight host now binds the instrument compositor to live player pose, engines, throttle, fuel, faults and stores; it supplies aircraft, mobile-surface and projectile contacts in native enumeration order. Both MFD scene pages use the recovered camera requests with the existing software world renderer. Original 1/2/3/R/N display controls are connected, fuel-status feedback returns to the system state, and pausing retains the completed frame rather than advancing instrument animations. The host requests indicator refreshes because it repaints the whole framebuffer.

The outer player callback's fuel-gauge/burn filter is also translated: **4,246 native cases agree**, including signed-word boundaries and the minimum burn estimate. This feeds the flight-data page's range estimate instead of inventing a fuel-flow approximation (`tools/check_fuel_gauge.py`).

A private X11 test exercises store and cannon firing, all three MFD key paths (including scene-camera pages), range/target keys, pause stability, designator controls and mission termination. **All 128 retail missions start with the live HUD**, and another pass advances **1,024 simulation steps in every mission under ASan/UBSan** before drawing. The existing 44 CTests and Release/HUD comparisons pass. These are integration checks, not complete native flight-frame comparisons. Static cockpit framing, upper aiming symbology, radio layout, full view selection and remaining flight-system bindings are still being connected; their diagnostic adapter remains visible.


### Default cockpit and instrument startup

```sh
python3 tools/check_cockpit_setup.py
python3 tools/check_cockpit_frames.py
```

The native viewport table begins at DATA `0x5da4` / file `0xf3848`. Its projection row, panel/overlay bindings, bottom edge and callback form a 20-byte record with the callback at offset 16, called by `0x73f4b`. The earlier research decoder began four bytes too early and associated each view with the preceding callback; it has been corrected in `tfx-reverse`. The preceding DATA `0x5da0` byte is a separate effect flag. **All 29 corrected native records agree.** The forward cockpit is view zero, with its original `infcock.hud` overlay, 55-row planar panel and instrument callback.

Native `0x7df30` overwrites the stored instrument defaults at mission startup. It selects flight data on the left, 50/100-mile radar ranges, full fuel gauge, fifteen chaff/flares and the lowered cockpit. The F-117 begins in stealth with a central map and right-hand systems page. Engine/gear indicators distinguish airborne and landed starts. **All six aircraft/start combinations agree.** Animated Page Up/Page Down follows the original 22-position sequence; **2,048 multi-frame steps agree**, including a full-height view interrupting motion.

The live host now paints that cockpit artwork and uses the native viewport projection origin. The existing approximate horizon respects the selected origin/focal length; the complete original sky/weather renderer remains separate. **198 complete pixel/coverage/offset/origin frames agree** against original composition across six views, three horizontal offsets and vertical clipping boundaries. The compiled overlays execute unchanged in the comparison. The VGA-to-VGA panel transfer is a declared hardware boundary: its original source/destination rectangle is applied to independently deplaned source bytes. Projection setup is checked with the clamped coordinates; this is not a complete pre-render camera-order comparison. The missile view's five-byte `.hdd` placeholder has no visible footer and is intentionally not decoded as a bitmap.


The setup, positioning and frame comparisons pass in development, Release and ASan/UBSan. Real-window MFD/designator/weapon/result checks still pass with the restored cockpit, as do all 44 CTests. Every retail mission starts successfully, including a further sanitizer run after 1,024 simulation steps per mission. The upper aiming and radio adapters remain the next visual integration work.


### Upper-HUD flight readouts

```sh
python3 tools/check_hud_text.py
```

The original numeric upper-HUD painter (`0x7f8e0`) and independent air-brake, brake-chute and arrester-cable labels (`0x7eb7c`) now have a dedicated renderer. Altitude, airspeed, Mach, load, map coordinates and throttle retain their original strings, word-coordinate wrapping, alignment and float/double storage boundaries. Watcom rounds a stored double after decimal scaling and suppresses the sign of rounded zero; host `printf` alone differs at values such as `0.005` and `1.015`. The reconstruction preserves this distinction for these unpadded readouts.

**768 complete native pixel/coverage frames agree**, including adjacent doubles at decimal-rounding boundaries, all independent message combinations, negative/positive telemetry and ten instrument/clipping offsets. Original formatting and VGA text execute in the comparison. This verifies the painters from supplied telemetry; their live upper-HUD dispatcher and remaining aiming symbology are still being connected.


### Heading strip and pitch ladder

```sh
python3 tools/check_attitude_display.py
```

The original forward-HUD heading strip (`0x80ffc`) and pitch ladder (`0x80ab8`) now render from their original sprite banks and 42 authored mark records. The ladder retains the 36-point, 1,820-angle-unit circle with three wraparound entries at each end, separate pitch/roll projection matrix, five visible marks, roll rotation, solid/dashed segments and VGA-group sprite clipping. Its projection remains distinct from the model renderer's camera matrix.

**1,024 complete native pixel/coverage frames agree**, including steep/inverted attitudes, table-step boundaries, independently varied projection angles and ten instrument offsets. The native angle setup, projection, clipping and all raster loops execute; only resource resolution supplies original bytes. This comparison also corrected the shared line clipper: native `0xbf53d` measures from the farther endpoint before truncating, which can move a clipped ladder edge by one pixel. Patterned lines preserve the original plane-mask phase and X-major final-endpoint behavior. Live dispatcher integration follows separately.


### Non-firing cannon prediction

```sh
python3 tools/check_cannon_prediction.py
```

The HUD trajectory helper `0xa2878` uses a private temporary cannon shot: five warm-up updates, then up to eight updates before each of five displayed samples, at twenty native ticks per update. Ground contact or an ended motion callback stops further advancement. The trajectory neither enters the live projectile pool nor consumes ammunition. The caller's movement-height bounds are explicit inputs; the earlier projectile comparisons supplied different bounds from the executable's stored defaults, so those controlled defaults are no longer assumed by this predictor.

**512 native trajectories and 2,560 Q8 samples agree**, including ground/ceiling boundaries, varied limits, inverted attitudes and negative/positive speeds. Original construction and motion callbacks execute with a supplied transformed launch anchor; screen projection is observed at its boundary. The native helper also restores its caller's elapsed timer. Mount transforms and projectile motion retain their separate comparisons; live aiming composition is the next consumer.


### Forward weapon HUD and live aiming integration

```sh
python3 tools/check_weapon_display.py
python3 tools/check_weapon_display.py --full
python3 tools/check_cannon_prediction.py
python3 tools/check_impact_prediction.py
python3 tools/check_window_controls.py --data-dir /path/to/TFX
```

Original navigation, air-to-air and air-to-ground layouts now use the authored sight masks, labels, ammunition text, range-dependent reticles, suppression cross, ARM/LOCK labels and pitch ladder. **768 complete native layout frames agree**, including all aircraft, cannon/stores, lock flags, independent projection angles and eight offsets. Another **768 forward-composition frames agree**, executing original `0x800f4` with supplied cannon/impact screen samples and declared exclusions for the acquisition and cannon-selection controllers. These cases omit selected target contacts, landing approach and temporary BINGO/PULL UP warnings; they establish composition in the supplied context, not a complete cockpit/frame claim. The heading strip and cannon trace change the line viewport inherited by the later pitch ladder; this context is now explicit.

The original Mk82 impact predictor (`0xa29dc`) also runs independently of live projectiles. It keeps the bomb at its mount during the initial attached stage, advances at ten ticks per update and stops at ground/lifetime termination. **256 complete native trajectories agree**, including varied height limits, inverted flight and boundary starts, with the caller timer restored. The native category-9 set contains the Mk82; other bomb guidance categories use their own consumers. The impact marker is drawn inside its authored bounds, while its connecting line is always clipped to the weapon viewport.

All **36 sight-mount lookups** agree with native `0x8b984`, including pylon -1 and the cannon sentinel. Those sentinels read adjoining stored records rather than a separately authored cannon anchor for every aircraft; their bytes are retained. Ordinary carried-store and firing-cannon mounts remain separate.

Live flight now paints the verified readouts/heading/weapon layout before cockpit artwork and lower instruments, replacing the temporary top-of-screen diagnostic labels. F returns to navigation; weapon selection also retains the cannon's selected air/ground role. HUD faults retain the original intermittent/failed gate. Sensor acquisition now uses the actual forward-cockpit projection origin. Projected trajectories currently use the existing approximate world camera and stored movement bounds; original world initialisation/projection and complete-frame/session verification remain open. Target outlines and radio placement still use their integration adapter. The private-window checks exercise firing, display cycling, scene MFDs, pause, designator controls and result handoff, and all 128 retail missions start successfully with the new forward HUD.

The forward composition, cannon/impact trajectories and sight-mount checks pass in development, Release and ASan/UBSan. All 44 existing CTests pass, and all 128 retail missions also draw after 1,024 simulation steps under sanitizers. These remain subsystem and integration checks rather than a complete original-session comparison.


### Scrolling radio messages

```sh
python3 tools/check_radio.py
python3 tools/check_radio.py --frames
```

The original radio controller (`0x81174`–`0x813c3`) now owns its message history, cursor, timing remainder and optional repeating pass. Messages retain both 20-character padding runs and native uppercase/digit filtering; the 19-character window is copied before advancement, and the final partial window is suppressed. Ordinary scrolling uses four ticks per character and time warp uses twenty. Clear, replacement and replay deliberately retain the timing remainder.

**4,096 native state/window updates agree**, including empty, mixed-case, punctuation, extended-byte and NUL-terminated messages, replacement, clear, replay, repeated messages and both timing modes. Another **512 complete native pixel/coverage/state frames agree**, including both original fonts, view-zero offsets and clipping. Original string preparation, controller and VGA font painters execute with the external pitfont bank supplied unchanged.

Live mission, aircraft and surface-unit radio events now feed this controller, replacing the temporary five-second full-line label. The coloured radio display renders after the auxiliary instruments at the original cockpit position. Plain L invokes the original replay branch; Alt+L's separate landing controller remains unimplemented. Paused frames retain the current window. Other camera modes still await their complete native callbacks.

The radio state/frame comparisons pass in development, Release and ASan/UBSan. The 504-frame original instrument-HUD comparison still agrees, all 44 existing CTests pass, and private-window firing, MFD, replay, pause and mission-result checks pass with live radio rendering.


### Target telemetry and marker painters

```sh
python3 tools/check_target_display.py
```

The original target-information painter (`0x7f59c`) retains the uppercased first word of the aircraft name, signed-word speed conversion, heading scaled by 360/65,536, unsigned altitude and horizontal range steps. The direction cue (`0x7f000`) preserves wrapped matrix products and native angle-helper behavior, including phase zero when either reduced input is within -1..1. Both sizes of primary/acquired masks are decoded directly from the executable. The edge-marker helper (`0x7ee9c`) clips toward the secondary marker's top-left corner before painting it; its caller uses this while the seeker is acquiring, rather than as a general minimum-range indicator.

**1,600 complete native pixel/coverage frames agree**, covering all five painter paths, names, signed telemetry, matrix overflow, projected/tracking points, eight cockpit offsets and four clip rectangles. The external aircraft-name lookup supplies text; native name conversion, formatting, geometry and VGA painters execute. Decimal readout formatting is shared with the already verified upper-HUD painter, and line clipping is exposed without changing its verified arithmetic. Live target-state composition follows separately.


### Live target composition and lock animation

```sh
python3 tools/check_target_tracking.py
```

The forward HUD now combines the original selected-aircraft data, heading chevron, target/direction markers and seeker-acquisition/lock overlays. The chevron rotates its three authored points by target heading minus player heading. The controller owns the retained normal/compact sight selection, nine-frame lock blink and previous-lock latch. Navigation draws retain this state rather than updating the latch. Failed/intermittent HUD gating remains in the calling flight host.

**1,024 original forward-HUD frames agree in pixels, coverage, retained animation and lock-cue requests** across selected/missing contacts, acquisition, confirmed/lost locks, suppressed radar cues, weapon changes, navigation, ground designation, offsets and resets. Original layout/controller/text/raster code executes. Sensor/selection state, world-screen projection and cannon/impact samples are supplied at their separately checked boundaries; audio/voice requests are observed without playback. These are supplied-state sequences, not complete simulated engagements.

The ground outline retains the original fixed X clipping limits and category-11 anti-radiation exclusion. Live flight binds actor profile names, Q8 position, heading, Q16 speed, player orientation and verified seeker state to these painters, replacing the temporary white tracking box and ground-target footer. Native navigation-contact cues, landing-approach guidance, temporary BINGO/PULL UP warnings and other view callbacks still need their own consumers. Lock-cue effects are returned for the pending audio backend.

The target-controller comparison passes in development, Release and ASan/UBSan, as do the 1,600 standalone target-painter frames. The existing 768 forward-composition frames still agree under sanitizers. All 44 CTests pass; private-window checks cover firing, display controls, paused target state and result handoff, and all 128 retail missions run 1,024 simulation steps under sanitizers.


### View selectors and transition queue

```sh
python3 tools/check_view_controls.py
```

The original ten function-key selectors now share an owned ten-entry view queue, retained cockpit offsets, low-memory alternatives, orbit matrices and fixed fly-past position. Intermediate viewport indices are named separately from final camera modes. Consumption occurs only after the current camera stage signals completion; a full queue drops further requests, and leaving a missile view discards the old queue. Shift+F1 and missile infrared retain the native save/restore rules. F5 deliberately has a different automatic-landing gate from the other keys. F9 compares target pointers before checking null, so two missing pointers still queue the ground-distance target view.

**9,690 native selector/queue updates agree** across all 29 initial views, all ten keys, blocked controls, low-memory mode, repeated requests, full queues, offsets and varied player poses. Original matrix rotations and fixed-camera placement execute; target selection is supplied at its external boundary and optional fly-past sound requests are excluded. Development, Release and ASan/UBSan comparisons pass. This establishes selection state; live camera motion and drawing are the next integration step.


### Head, chase and target camera integration

```sh
python3 tools/check_view_cameras.py
python3 tools/check_view_controls.py
python3 tools/check_window_views.py --data-dir /path/to/TFX
```

The camera dispatcher now retains the original head lift, yaw/pitch limits, head-turn stages, return-to-panel movement, chase smoothing/ground limit, fixed fly-past placement and automatic chase handoff. Tactical and target-observer cameras consume explicit selected contacts. Virtual-cockpit target tracking retains its original directional limits, roll gates, lost-contact recovery queue and automatic resumption. The view-matrix generator and Q16 orbit-position helper are shared with the separately verified arming camera. Full-world scene positions are unwrapped around the native terrain anchor without changing the original wrapped Q16 translation.

**11,776 native dispatcher updates agree** across twenty-three view modes (including the missile extension below) in retained state, complete camera matrices, Q16 positions, automatic queue changes and internal/side/exterior player-model requests. Cases include rotated world/orbit matrices, signed-angle wrapping, pan input, ground limits, missing contacts and target loss. Original camera/controller/matrix code executes; supplied player/target poses and ground heights are explicit inputs, while navigation contact selection, world-driver configuration, weather advancement and actual player-model rendering are declared boundaries. The native target observer's variable-shift height-anchor quirk is retained independently of its camera translation.

The comparison passes in development, Release and ASan/UBSan, as do all 9,690 view-selector updates. All 29 viewport records and the existing 198 cockpit-composition frames still agree. The existing 12,860 arming mount picks and 60 native arming-animation draws also pass after the shared-math change. Private-window checks cover chase/full/head/rear/tactical/target/virtual keys, cockpit-offset restoration, paused-frame retention, existing firing/MFD/designator controls and result handoff.

Live flight selects the appropriate forward-HUD callback, preserves radio placement in other views and restores cockpit offsets after transitions. F1–F10 are connected, including the missile extension below; recovery/ejection cameras, the separately restored external-view information painter and complete frame/session verification are described below. Inspection also exposed the model-scale clipping limitation corrected below.

All 44 CTests pass after rebuilding every development target. These camera-state checks remain distinct from complete world-frame equivalence.


### Model-scale clipping and virtual canopy

```sh
python3 tools/check_orbit_camera.py
python3 tools/check_scaled_clipping.py
python3 tools/check_window_views.py --data-dir /path/to/TFX --mission 601
```

The virtual-canopy models use an authored camera scale of 400. The earlier scene painter converted their vertices into world units but discarded that scale before near clipping, removing the entire canopy. Vertices now retain world units per native camera unit through polygons, lines, particles, carried-model attachments, facing tests, clipping and projection. The factor between Q15 model products and the Q16 camera loop is explicit. The original opaque canopy and static MFD textures consequently render in the live virtual and head-turn views.

Original integer projection restores the native Q16 coordinate grid after unit conversion. This avoids a floating-point rescaling artefact where an exact depth such as 29 became 28.999999999999996 and selected the wrong integer divisor. No palette interpolation or artwork resampling is introduced.

The existing **4,121 native matrix/orbit cases, 4,096 native model projections and 4,096 native model-vertex products agree**, with **36,864 additional rescaled projection cases** including exact depth boundaries, fractional signed coordinates and canopy units. A separate property check compares complete software output for **1,287 polygon/line/point cases** across nine unit scales, including near-plane crossings; this establishes clipper scale invariance, rather than claiming original complete-raster equivalence. Carried-model tests also check inherited and replaced projection units.

These checks pass in development, Release and ASan/UBSan. All 44 CTests pass. Private-window camera/offset/pause checks pass in missions 501, 601 and 701; scratch captures permit visual inspection of the authored canopies. All 128 retail missions run 1,024 simulation steps and draw under sanitizers with the corrected path. Complete world transform/order, clipping/raster boundaries, camera callbacks and session equivalence remain separate work.


### Native world-point projection

```sh
python3 tools/check_point_projection.py
```

The original HUD/world-point projectors (`0xc5759` and `0xc57d1`) now have explicit integer-coordinate and Q16-coordinate consumers. They retain word-relative subtraction, separate signed high products for fixed coordinates, wrapped matrix rows, the inclusive 16–20,000 camera-depth range, truncated reciprocal and signed high-product screen conversion. These paths differ from the polygon vertex projector. Invalid points retain the original `(-1,-1)` result at live call sites.

**16,384 native cases agree**, covering both routines, arbitrary stored matrices, coordinate/angle-origin wrapping, fractional camera positions, near/far boundaries and negative screen rounding. Original matrix/translation setters and both complete point routines execute without intercepted dependencies. Another 182 compact synthetic native cases run in the asset-free CTest suite.

Live acquisition uses the verified projector with the existing forward-camera adapter. Cannon/Mk82 prediction and designated-ground outlines use the active camera's stored matrix, Q16 position and projection origin. Predicted Q8 coordinates are shifted to integer world coordinates before the word projector, as in the original callers. Complete sensor/camera lifetime ordering, navigation/approach consumers and full-frame/session equivalence remain separate integration work.

The native comparison passes in development, Release and ASan/UBSan. All 45 CTests pass; private-window checks still cover air/ground firing, acquisition-related controls, radar/MFD pages, designation, pause and result handoff.


### Fired-store and impact cameras

The F8 camera follows the latest player-fired store, excluding cannon and countermeasures, as selected by native `0xa2f1b`. A second press enters its forward/nose view and uses the original missile overlay. The follow camera retains the last projectile pose, original Q15 orientation composition, one-fifth position smoothing, pan increments and distance growth after the projectile ends. Its queued handoffs lead to tactical, impact-orbit or fixed fly-past views. The current live impact observer follows the first fatally damaged aircraft until its retirement; surface/scenery/player-crash observer lifetimes still need their selecting consumers.

The expanded native dispatcher comparison covers **23 modes and 11,776 persistent updates**, including live/missing missiles, valid nose views, impact contacts, queued fallback, all previously covered camera modes and their model selections. All **9,690 selector updates** still agree. These comparisons pass in development, Release and ASan/UBSan. All 46 CTests pass, and private-window checks now fire a store, enter both F8 views, return with F1 and continue the existing cannon/MFD/designator/result checks.

One original unsafe case is handled explicitly: the nose-camera dispatcher queues its follow-view fallback when the missile is missing, then continues to read through the null missile pointer. The C++ host retains the last valid camera pose during that handoff. This boundary is covered by an asset-free lifecycle regression rather than claimed as a defined native-memory comparison. The original infrared palette, other lighting changes and remaining camera artwork remain separate renderer work.

The preceding native world-point integration also passes the full 128-mission, 1,024-step ASan/UBSan run after its live caller changes.


### External-view flight information

```sh
python3 tools/check_external_display.py
```

The external-camera callback now draws its original altitude/speed/heading strip. The panel retains underlying palette bits through AND-15 / OR-128 operations, then paints the original borders and seven-row fixed-font labels with one-pixel shadows. Altitude and speed products retain their original stored-double formatting boundary. This uses the presentation fixed font, independently of the smaller forward-HUD font. Live chase, tactical, fly-past, target and impact views receive the current player telemetry.

**512 complete original pixel/coverage frames agree**, including enabled/disabled strips, positive/negative telemetry, heading wrap and all 256 background indices. The original polygon scanner, VGA logical operations, formatter and fixed-font painters execute; the already checked radio consumer is disabled at its own boundary. The comparison covers the full 320×200 external viewport, rather than claiming arbitrary clipped-panel behavior.

The comparison passes in development, Release and ASan/UBSan. All 46 CTests pass, and private-window checks exercise the external view transitions and telemetry strip alongside cockpit restoration and paused-frame retention.


### Infrared and night-view palette selection

```sh
python3 tools/check_palette_selection.py
python3 tools/check_environment_palettes.py
python3 tools/check_window_palettes.py
```

The original selector now owns its normal-palette dirty flag and infrared latch. The designator requests infrared directly. Missile nose view tests the original active-projectile-list condition independently of weapon guidance type. Night vision also checks the environment/night/request flags and preserves the original exclusions for particular external/transition views. Repeated requests suppress redundant uploads; a changed normal palette stays pending while infrared is displayed.

**5,952 native selector updates agree** across all 29 views, every eligibility combination, dirty/infrared initial state and persistent switching sequences. The original selection tail executes unmodified with its saved-register stack supplied; hardware upload is observed at its external boundary. Altitude, flashes, blackout/redout generation and full session ordering remain separate work. Another **441 compact native cases** run without game assets in CTest.

Lighting configuration now owns the five authored altitude palette names/boundaries, infrared palette, schematic and night status independently of terrain loading. All bindings agree with **33 independently captured native environment parses**. Keeping this separate also permits checking configurations whose names are not valid standalone terrain grids. Live flight uses these infrared bindings for missile/designator views and Shift+N night vision, including normal-colour restoration in the original excluded cameras. Private-window tests exercise Shift+N, day/night eligibility, chase-view exclusion and cockpit restoration, checking the displayed colours against the actual palette banks.

Development, Release and ASan/UBSan selector/configuration comparisons pass. All 47 development CTests pass. Complete scene lighting/remaps, altitude palette progression, weather and complete-frame/session equivalence remain open renderer gates.

All 128 retail missions also run 1,024 simulation steps and draw under ASan/UBSan after the lighting-configuration separation.


### Altitude palettes, protected colours and the VGA DAC

```sh
python3 tools/check_palette_lighting.py
python3 tools/check_dac_upload.py
python3 tools/check_window_palettes.py
```

The live world palette now follows the original five altitude banks and boundaries. Native `0x73214` selects a bank pair from the negated camera-anchor Y, retaining integer 50-stage blending, truncated divisions, the shared cached step and target-view exclusion. The fixed-colour overlay retains its pitch/roll phase, executable-backed colour entries and magenta leave-unchanged sentinel. These phases use the original player pitch/roll inputs separately from the camera altitude. Live startup also retains the original black first palette entry.

The flash/redout/blackout palette painters preserve their native retained flash byte, update gates, byte wrapping and effect-sample requests. The original blackout branch's second sample threshold reads the redout word; this quirk is kept. **4,224 original updates agree in every palette byte, retained step/phase/flash value and effect request across all 33 environments**, including boundary heights, repeated state, target view, reserved colours, disabled effects, signed effect levels and high-bit HUD values. Original startup and complete palette helpers execute; authored banks and view/effect state are supplied. The check stops before the separately verified upload selector. Darkside planet blending and the controllers which produce flash/blackout/redout state remain outside this comparison; live flight currently binds the altitude/fixed-colour path, rather than inventing those effect triggers.

Tracing the upload also corrected an earlier research interpretation of `HUDCOL`. It is the green component of palette entry 6, with a compiled value of 125 and original Y/U clamp of 90–127. Native `0xc770c` uploads these bytes unchanged; the six-bit VGA DAC discards their high two bits. Live palette expansion therefore models DAC masking, while external palette files still reject components greater than 63. The native differential uploader and a six-bit port model agree with C++ RGB presentation for **304 updates and 227,352 component writes**, including all byte values, the HUD range and repeated unchanged palettes. Another 26 compact native DAC cases run without original assets in CTest and retain strict asset-file validation.

Y/U now change brightness at the original flight-input cadence and preserve the native dirty-palette request even when both keys cancel each other. Private-window checks observe the default green level and both clamps in the displayed HUD, alongside night-vision/camera palette handoffs. Atmospheric geometry, weather, colour remaps, effect-state producers and complete world-frame/session equivalence remain separate work.

The palette and DAC comparisons pass in development, Release and ASan/UBSan. All 48 development CTests pass. All 128 retail missions run 1,024 simulation steps and draw under sanitizers with the new live palette path.


### Temporary BINGO and PULL UP HUD warnings

```sh
python3 tools/check_hud_alerts.py
python3 tools/check_target_tracking.py --alerts
```

The temporary HUD line tables previously described as launch notices are the authored **BINGO** and **PULL UP** messages. Their owned state retains the selected shape and original 108-tick timer. A request replaces the current shape and restarts that timer. Each actual draw subtracts its supplied elapsed word, with wrapping before the signed expiry test; a skipped HUD draw does not consume time. The original painter sends direct VGA lines without clipping, so comparison offsets are restricted to valid physical-screen positions. Changing the line viewport does not clip these messages.

**512 complete original frames agree in pixels, written coverage, selected message and retained timer**, covering both shapes, the fuel threshold and adjacent doubles, repeated/no draws, requests, valid offsets, ignored viewports, expiry and long-tick wrapping. The original BINGO request executes before the failed/intermittent HUD gate and uses the executable-backed fuel threshold of 500. The native unsafe out-of-screen line-write context is excluded explicitly.

Another **1,024 complete original forward-HUD frames agree** with warning requests, paint order and timer included alongside all existing target/lock animation and event checks. NAV paints its warning inside its own layout, before the outer brake/chute message repeat; AA/AG paint it at the end. Refuelling contact selection and terrain-risk producers remain separate. Live flight now supplies low fuel before its HUD-fault gate and elapsed ticks to the painter. The PULL UP shape/request/painter is verified, while its actual terrain-risk trigger still needs its native selecting consumer.

The warning painter and full-composition comparisons pass in development, Release and ASan/UBSan. All 48 CTests and private-window camera/pause checks pass. All 128 retail missions run 1,024 simulation steps and draw under sanitizers after the live BINGO integration.


### Aerial refuelling and tanker HUD

```sh
python3 tools/check_refuelling.py
python3 tools/check_view_cameras.py --refuelling
python3 tools/check_target_tracking.py --alerts --navigation
python3 tools/check_window_refuelling.py --data-dir /path/to/TFX
```

The original apostrophe handler now opens/closes the player aircraft's refuelling equipment, saves the current NAV/AA/AG mode and forces NAV until it restores the saved mode. The camera-stage controller searches the fifty allied slots for the nearest eligible tanker, retaining original tie order and wrapped distance arithmetic. Alignment, reconnect delay, speed gates, port anchors and coupling divisors follow native `0x94efc`, `0x94e34` and `0x950f4`. The original coupling moves the tanker towards the player's port. Refuelling adds the executable's 14 fuel units per player update, caps at aircraft capacity and requests the original afterburner warning. Connected refuelling inhibits cannon and carried-store release and selects the corresponding aircraft model entries.

**6,144 persistent original-code updates agree** in all controller state, target selection, tanker poses, exact fuel bits and emitted requests. The cases cover all three aircraft and reach 46 connections from approach, with 882 coupled frames and 1,289 tanker updates. Original controller, bearing, anchor and distance code executes. SSD category lookup is supplied from separately checked resource semantics; model-entry, audio and voice requests are observed at their external boundaries. Fuel transfer executes its original player-update prefix before the camera controller, preserving the previous phase's lifetime. These checks do not establish complete flight-session ordering or collision protection.

The camera comparison also agrees across **11,776 updates in 23 modes with four supplied refuelling phases**, preserving the original phase addition to player model entries. The live tanker HUD now draws its direction cue, telemetry and marker before weapon acquisition, sharing the retained projected point and sight. This optional path consumes its own BINGO/PULL UP warning draw before the later weapon layout. **1,024 complete original composed HUD frames agree** in pixels, coverage, target/lock state, warning timers and events with this contact path enabled. Sensor/render lifetime ordering outside this supplied sequence remains a separate gate.

Controller, camera and HUD comparisons pass in development, Release and ASan/UBSan. Private-window checks exercise real apostrophe input, forced NAV, saved AA/AG restoration, pause and camera return. All 48 CTests pass. The complete 128-mission sanitizer pass still advances 1,024 steps per mission and draws. Live sample/voice playback awaits the audio backend; ILS, automatic landing and non-Arcade flight models remain separate work.


### Instrument landing system

```sh
python3 tools/check_landing.py --selection
python3 tools/check_landing.py
python3 tools/check_target_tracking.py --alerts --navigation --landing
python3 tools/check_window_landing.py --data-dir /path/to/TFX
```

I/backslash now uses the original ILS toggle, including its inhibition gates, NAV selection, control-setting results and damage-dependent voice request. The destination selector checks the player route's last point, then its first. It accepts a runway SSD at that cell or searches allied surface slots in descending order for an eligible carrier. Actor retirement and the carrier SSD's destroyed state are distinct checks. Destroyed-carrier and missing-runway messages retain their native order. Landing ports use the first three initialized SSD custom words; the approach beacon uses the original integer movement helper and 20,480-unit setback.

**1,536 complete original key/selection executions agree**, including all three aircraft, ordinary/realistic control coefficients, gates, empty routes, missing runways, destroyed carriers, exact control-setting bits and voice/radio requests. The native waypoint accessor and surface enumerator execute. Terrain handles, initialized SSD port/type/damage fields and audio requests are declared external boundaries. This comparison includes 252 runway and 654 carrier selections; it does not claim a complete native world-load/session trace.

The HUD first directs the player to the beacon, then computes the original lateral and vertical corrections for runway approach. The retained corrections and height status follow actual HUD draws. Land-based offsets use the native extended-precision product and truncation; carriers retain their authored offsets. **1,024 complete native guidance frames agree** in every pixel, written coverage, state word, projected-point input and radio request. They include 308 beacon-to-runway transitions and directed high, low and aligned approaches. The world-point projector is supplied at its independently verified boundary. A compact set of **61 original state captures** runs in CTest without game assets.

The combined forward-HUD comparison also agrees across **1,024 complete frames** with targets, refuelling contacts, BINGO/PULL UP warnings and ILS enabled together. It checks the original removal of NAV/ammunition labels, guidance placement, warning timing and retained landing state. Live flight binds original route data and SSD custom fields to this path. Private-window checks exercise both keys, forced NAV, subsequent weapon modes, pause and external-view return. Automatic landing, remaining flight-model consumers of the returned control coefficients and the autopilot's speed/altitude request producer are still separate work; audio requests await playback.

The ILS controller, complete guidance and combined HUD comparisons pass in development, Release and ASan/UBSan. All 49 CTests pass. All 128 retail missions also advance 1,024 steps and draw under sanitizers after integration.


### Automatic campaign return after flight

```sh
python3 tools/check_campaign_return.py --data-dir /path/to/TFX
```

Native launcher `0x72588` resumes its caller after the flight loop, result processing, FINISH and teardown; it has no extra diagnostic results wait. Campaign flight now returns as soon as FINISH completes, allowing the existing parser to select its original result, recovery, life-loss and next-mission presentations. Direct `--mission` launches retain their standalone diagnostic results page.

A private-window integration check restores a disposable native-format pilot into the final Arcade mission with one life, uses the original arming screen, reaches aircraft loss through normal real-time simulation and observes the automatic return to the authored Arcade presentation. No damage, result or FINISH variables are injected. Shift+Q then exits the presentation normally. This tests the assembled return path; the original parser/rule subsystem comparisons remain its independent evidence, rather than a claim of complete native session equivalence.


### Ejected-pilot motion and down-axis alignment

```sh
python3 tools/check_down_alignment.py
python3 tools/check_ejection.py
```

The pilot (`0x8f198`) has distinct seat launch, powered-seat flight, fall, parachute opening, suspended descent and landed-wait stages. Its typed state retains float position/velocity, the Q15 orientation, stage timers, chute state, collision/termination and fatal-impact latch. Model events remain explicit outputs. Native float storage boundaries, extended intermediate products, signed-word pose handoff, wrapped counters and invalid integer-conversion results are preserved.

**12,288 complete original controller updates agree** in exact stored float bits, integer pose, orientation, angles, timers, stage/termination state and ordered model-entry requests. These include eight persistent trajectories and synthetic phase/timer/impact boundaries. The original phase helpers, orientation code and pose handoff execute; terrain classification/height, world-water status and timer increments are supplied. SSD animation requests are recorded separately. **141 compact original cases** run without game data.

Pilot descent and its camera use duplicate native down-axis builders (`0x8e86c` and `0x91c48`). One shared C++ implementation retains their float normalization and cross-product stores, separately from the existing integer attitude normalizer. **4,096 complete original matrix updates agree across both copies**, including rejected small/degenerate axes; **121 cases** run without game data. Both comparisons pass in development, Release and ASan/UBSan, and all 51 CTests pass.

These are controller kernels. The Escape input sequence, animation-triggered player/pilot handoff, abandoned-aircraft motion, ejection camera, recovery-region classification and live outcome selection are the next integration consumers; this evidence does not establish a complete ejection session.


### Escape input, ejection camera and actor handoff

```sh
python3 tools/check_ejection_sequence.py
python3 tools/check_view_cameras.py --refuelling
```

The original two-press Escape handler now has a typed sequence controller. Confirmation clears refuelling enable, requests aircraft entry 11 twice, prepares the fixed camera position and emits the original camera/voice/sample requests. Seat release is a later SSD animation event, rather than an immediate change to pilot physics. The frame boundary snapshots the abandoned aircraft's Q8 position and basis, switches to the pilot and continues the aircraft's straight-line motion independently.

**4,096 complete original key/frame-boundary cases agree** in Escape wrapping/counts, ejection stage, handle-switch decision, fixed camera, abandoned-aircraft pose and ordered model/camera/audio requests. The full key handler, movement/bearing helpers and native `0x70ec8` execute. Player physics and the SSD seat-release event are supplied at their separately checked boundaries; resource identities and output consumers remain explicit.

Viewport 21 is now named for ejection. Its camera uses the real float player position, the original twelve-unit setback and final Q8-to-Q16 shift, including near-ground down-axis alignment. Both the player-data basis and renderer basis are explicitly supplied in the native harness. **24 modes and 12,288 persistent camera updates agree**, retaining all prior camera and refuelling-phase coverage. Key/handoff and camera comparisons pass in development, Release and ASan/UBSan. Live Escape, animation-event delivery and the remaining recovery/outcome consumers are still being connected.

### Live ejection, recovery regions and campaign outcomes

The aircraft SSD's opcode 33 now emits an explicit seat-release event through the scene animation boundary. Double Escape requests the original aircraft entries and camera; the later authored animation switches to the separately loaded man/pilot assembly. Pilot physics runs before the abandoned-aircraft update, preserving the original frame order. Collision observations feed the next pilot update, while fatal aircraft handling remains separate. Engine, weapon, refuelling and view controls retain their ejection gates. The guided-missile input formerly named player invulnerability is now correctly identified as the native ejection flag.

Recovery executes before FINISH. Orders violations bypass the map, undamaged aircraft abandonment can dismiss the pilot, and hostile map index 2 produces missing-in-action status. The original starting-cell marker overrides that map when the pilot returns to the starting cell. Native region order is Somalia, Colombia, Libya, Yugoslavia and Atlantic. The executable also names a fallback noncap.lbm that is absent from retail 2.05; the reconstruction reports that missing resource only if an unknown region actually needs it. Out-of-image positions receive missing-in-action status as a bounded host policy; the native comparison covers valid image coordinates only.

The complete original launcher prefix and recovery routine agree in 3,072 outcome/status/safe cases across all five retail maps. Parser variable lookup, indexed screen upload and VGA reads are supplied boundaries; the original starting-cell pixel write executes. All 33 environment bindings agree with independent native captures, including water-world flags for both sea and seastorm. The SSD comparison still agrees across all 703 definitions and 19,589 persistent interpreter states.

A private-window check sends real Escape keys and observes the pilot controller using GDB without modifying state. One press retains flight, the second starts the authored aircraft animation, the seat-release event launches the pilot, the parachute opens, and a further Escape executes recovery and FINISH with outcome 3. Shift+Q currently quits the run directly; the original in-flight configuration/quit menu remains a separate pending consumer.

    python3 tools/check_recovery.py
    python3 tools/check_window_ejection.py --data-dir /path/to/TFX

The live ejection change also passes all 51 tests in Release and ASan/UBSan, the private-window ejection sequence under sanitizers, the ordinary campaign restore/arming/loss/FINISH/presentation return, and all 128 retail missions with 1,024 simulation steps plus rendering each.

### Simple flight-model control response

The Simple model has a separate G-limited response stage at native 0x8cfa0. It retains pitch/heading/roll rates and derived positive/negative pitch and roll limits. Aircraft G limits, stall speed, the prior coordinated-turn contribution, attitude and the Alt modifier are explicit inputs. Keyboard decay and competing-key priority, joystick threshold flags, Alt-modified rudder steering and low-speed ground rules remain distinct branches. The low-speed formula uses a literal nine-G coefficient even for the F-117's six-G preset; that native quirk is preserved.

The full original routine, its joystick-direction helper, x87 normalisation and power routine agree across 8,192 exact float-bit/rate/limit/key updates. Coverage includes all input modes, the 150 m/s boundary, signed-zero joystick arithmetic, ground handling, key priority and wrapped maximum elapsed ticks. There are 123 compact original captures in the asset-free test suite. Development, Release and ASan/UBSan comparisons and all 52 CTests pass.

    python3 tools/check_simple_controls.py

This is a verified response stage, not yet a live selectable Simple model. Bank-turn, ground handling, movement and the outer flight-model dispatch remain integration consumers.

### Simple coordinated turn and G load

Native 0x8d3cc combines attitude-weighted bank compensation, a retained correction word, two heading rotations through the shared residual, Mach-dependent turn response, and movement/speed/G stores. The C++ stage keeps the original transpose order, signed-word sector boundaries, sine limits and every float/double storage boundary. Its separate body basis is important: the later low-speed visual attitude does not replace the physics orientation.

All 8,192 complete original matrix/residual/correction/speed/G cases agree without intercepted dependencies. They cover both sides of the bank sectors, the Mach 0.1 threshold, elapsed-word limits and prior heading residuals. Development, Release and ASan/UBSan comparisons pass; 123 compact captures bring the full CTest suite to 53 passing tests.

    python3 tools/check_simple_turn.py

The coordinated-turn and control-response stages are still being assembled with ground handling and the outer Simple flight dispatch.

### Simple low-speed attitude and touchdown

The native stall-pitch stage (0x8d6e8) updates the visible basis separately from the body orientation, retaining its signed-word pitch, per-tick movement and distinct extended/double/float comparison boundaries. Ground contact (0x8d970) preserves sink below stall, gear clearance, inverted/gear-up impacts, filtered touchdown pitch/roll, previous-landed state, low-speed rolling contact and the carrier arrestor-wire latch.

All 8,192 complete original cases agree in both matrices, rotation residuals, altitude, contact/landing flags, signed words and speed/reference stores, without intercepted dependencies. The cases include independent and consecutive stage calls, gear-height boundaries, stall/reference thresholds, maximum elapsed words and prior contact state. Development, Release and ASan/UBSan checks pass; 123 compact native cases bring the normal CTest suite to 54 passing tests.

    python3 tools/check_simple_ground.py

### Landing-gear damage

Native 0x8d7ec computes descent speed from the body's forward axis, applies aircraft-specific touchdown/rolling/airborne limits, distinguishes intermittent gear damage from complete failure, retracts failed gear and emits ordered sample requests. The thresholds retain the native factors of two for landing descent and 1.4 for speed; the same frame can warn for ground speed and subsequently warn or fail for airborne speed.

All 8,192 complete original state/float-bit/audio cases agree, with sample delivery as the only intercepted boundary. They include healthy and already damaged gear, first and continuing ground contact, exact/adjacent speed thresholds and every resulting request sequence. Development, Release and ASan/UBSan checks pass; 123 compact native cases bring the CTest suite to 55 passing tests.

    python3 tools/check_landing_gear.py

### Complete ordinary Simple flight callback

The assembled Simple update retains the native mode-entry conversion from engine/start speed 250, the slower non-Arcade speed response, doubled fuel consumption relative to Arcade, input response and bank-turn ordering, body/visible basis separation, shared rotation residuals, normalization cadence, stall appearance, movement, gear damage, touchdown, single-step city wrapping and the final outer contact check. Basic aircraft limits are gathered beside their shared Arcade/Simple consumers.

The full initialized callback agrees for 36 persistent trajectories and 15,360 original updates across all three aircraft. Keyboard and joystick inputs, Alt-modified rudder, engine changes, gear operations/damage, high-altitude recovery, city seams, runway and carrier starts, and engines off/on are covered. Native player/start initialization and the complete ordinary 0x8f56c callback execute; timer, input acquisition, the independently verified HUD/control setup and scene/audio notifications are declared boundaries. Initial ground placement executes before supplied engine/control changes. Automatic recovery, ILS and the separate disabled/pilot dispatcher paths are excluded from these ordinary Simple trajectories.

Integration exposed two distinctions obscured by the prior local-stage names. The byte at raw key offset 0x38 is Alt, rather than an input-device capability. Data 0x8d12/0x8d14 are the arrestor hook and arrested state, not the brake chute; H's native handler and carrier zone 3 establish their use. Gear position and gear indication are also separate: the basic speed calculation applies gear drag only while the indication equals active, even when a damaged gear remains physically down. These native behaviours are retained.

All 15,360 updates agree in exact position/pose, both bases, residuals, Mach response, controls/limits, gear, fuel and flags in development, Release and ASan/UBSan. The 36 compact trajectory captures bring CTest to 56 passing tests. Sharing the basic speed/fuel code also retains all 4,608 Arcade airborne and 9,216 ground-start/takeoff comparisons unchanged.

    python3 tools/check_simple.py

Live model selection and the remaining disabled-flight/camera consumers are being connected separately.

### Live Simple model, damaged flight and separate camera bases

The live selector accepts --flight-model=arcade or --flight-model=simple, preserving Arcade as the default. Simple binds its original keyboard response and Alt-modified rudder; the native non-Arcade menu disables mouse steering. Input remains separate from numerical updates. The live loop propagates gear indication/damage, authored gear/chute model entries, H's arrestor-hook state and the SSD contact zone used by carrier arresting logic.

The shared disabled-player callback now selects the correct basic speed response for each model. Another 36 persistent trajectories and 15,360 complete original updates verify the switch after 64 ordinary Simple frames. Both ordinary and disabled comparisons separately observe the physical basis, retained visible basis and actual renderer basis. Displayed G load is also separate: native disabled flight clears 0x133f4 while retaining the Simple calculation at 0x133c4. Both 15,360-update comparisons pass in development, Release and ASan/UBSan.

Camera contexts retain the current renderer basis and can borrow a distinct physics basis. Ejection uses the physics basis; ordinary view composition retains the renderer basis. All 24 supported modes and 12,288 persistent original updates still agree with deliberately different bases and every refuelling phase. The live seat-release frame also retains the pilot's physical basis independently from that frame's visible aircraft pose.

All 57 CTests pass in development, Release and ASan/UBSan. Every one of the 128 retail missions runs 1,024 steps and draws under sanitizers in both Arcade and Simple. Private-window checks verify real Simple Down/Alt+Left/H input, authored seat release, parachute opening and recovery/FINISH; the same sequence passes under sanitizers. Existing ILS, MFD, weapon release, cannon, designator, camera and mission-result window checks remain passing.

    python3 tools/check_simple.py --disabled
    python3 tools/check_view_cameras.py --refuelling --split-basis
    python3 tools/check_window_ejection.py --flight-model simple --data-dir /path/to/TFX

These checks establish the connected basic-model paths and their declared boundaries. Realistic/Military Spec dynamics, automatic recovery/landing, complete native input acquisition, remaining contact/rendering consumers, full campaign coverage and audio remain unfinished.

### Aerodynamic physical-frame to world/view bridge

The Realistic and Military Spec pipeline retains a North/East/Down physical frame in metres, distinct from the renderer's East/Down/North coordinates. Native 0x95aa0 normalises its Q15 basis, permutes both the local axes and world coordinates, applies the aircraft-specific eye height and the current forward/upward cockpit offsets, then exports float and integer positions, renderer basis and angles. The double normalization constant and intermediate double stores are preserved independently from the basic models' float normalization.

All 8,192 complete original cases agree in normalized physical basis, both exported bases, exact float bits, integer coordinates and angles, without intercepted dependencies. Coverage includes arbitrary rotations, all three aircraft, signed-zero positions, variable viewpoint offsets and the original integer-indefinite result for out-of-range conversion. There are 123 compact asset-free captures. This is the shared pose bridge; the aerodynamic integrator is still being reconstructed.

    python3 tools/check_aerodynamic_pose.py

### Aerodynamic atmosphere and physical motion

The original atmosphere stage (0x992fc) is reconstructed with its altitude-dependent density correction, the floor applied only above ten kilometres, optional surface temperature, capped temperature lapse and temperature-derived sound speed. It preserves the original 273-degree Celsius offset and each double storage boundary. All 8,192 complete original density/temperature/sound cases agree without intercepted dependencies, including both temperature settings and altitude/lapse boundaries.

Both physical motion callbacks (0x9f9d4 and 0x9fbec) retain the half-acceleration position correction, airborne-only fivefold time warp, original crash-disabled fallback, distinct roll/pitch/yaw rotation paths and double roll/pitch remainders. The simplified branch still rotates by the commanded roll even when its stored roll rate is inhibited, as the original does. All 8,192 complete original position, velocity, rate, remainder, collision and Q15-frame cases agree without intercepted dependencies. Force, command and timestep production remain explicit input boundaries; these local comparisons do not establish a complete Realistic/Military Spec session yet.

    python3 tools/check_atmosphere.py
    python3 tools/check_aerodynamic_motion.py

### Aerodynamic load summation, inertia and x87 verification boundary

Native 0x9f128 sums aerodynamic, engine and ground body forces/moments, adds gravity through the physical NED basis and exports the world force and G load. It retains the refuelling connection's levelling moment and the fully arrested carrier branch, including its signed-zero body-velocity stores and retained down motion. Native 0x9f3b0 then derives linear and angular acceleration, including the fuel-squared roll-inertia term. All 8,192 complete original force, moment, acceleration, G-load and arrested-velocity cases agree with the declared hardware FPATAN boundary below; 123 compact captures are retained.

This exposed a limitation of the installed Unicorn build: its helper_fpatan_x86_64 narrows both operands to binary64, calls the host double atan2, then widens its result. Disassembly of that library and direct 80-bit-register captures establish this independently of the game's equations. The capture adapter now replaces only the four Watcom FPATAN instruction sites with a real host x87 instruction, preserving register words, tags, TOP and the controlled FPU word. All surrounding original Watcom range reduction, square root and force arithmetic still executes. The current x87 backend uses the same instruction and the original asin-via-atan evaluation order; no binary64 emulator artefact is deliberately added to the game. The shared verification adapter is test-only, with no runtime game dependency.

    python3 tools/check_aerodynamic_forces.py

Full aerodynamic force producers, control response, ground suspension and initialized Realistic/Military Spec trajectories remain subsequent consumers of these verified stages.

All 61 CTests pass in development, Release and ASan/UBSan. The atmosphere, motion and force stage comparisons also pass in all three builds. These captures retain the controlled 0x037f FPU environment; tracing the actual Watcom startup environment remains necessary before claiming full-session arithmetic agreement.

### Original Watcom startup precision

The original FPU detection/reset path, 0xde851 -> 0xde81e -> 0xea0fc, now executes before numerical capture. Without intercepted dependencies it detects the 387 branch (data 0xacc8/0xacc9 = 3) and loads the compiled control word 0x127f: masked exceptions, round to nearest and **53-bit arithmetic precision**. This supersedes the explicitly controlled 0x037f/64-bit environment used in earlier local numerical checks. An instruction scan identifies the remaining control-word writes as integer-conversion, power-helper rounding, reset and exception-handler paths, rather than another game-selected arithmetic precision.

The numerical flight entry points now scope that native environment and restore the host word on return, including nested calls and exceptions. The host window and libraries retain their own environment. The 387 asin path uses actual FPATAN directly after the original square-root expression; the capture adapter still supplies hardware FPATAN instead of Unicorn's narrowed implementation. New atmosphere, force, physical-motion and pose captures are regenerated under the executed startup settings. Simple local stages, gear damage and all 1,197 store loadouts and three aircraft presets agree under these settings too. Complete basic-model trajectories are checked separately before considering this audit complete.

    python3 tools/check_fpu_environment.py

Under the executed startup settings, complete Simple ordinary and disabled trajectories each retain exact agreement for 15,360 updates; Arcade retains 4,608 ordinary and 4,608 disabled updates, plus 9,216 ground-start/takeoff updates. The basic-model trajectory fixtures and affected stage captures are regenerated from the original under that environment. Development and Release each pass all 62 CTests, including explicit precision, nesting, host-restoration and exception-restoration checks.

The same startup-precision audit passes all 62 tests and both complete 15,360-update Simple comparisons in ASan/UBSan as well. Release repeats of the advanced atmosphere, force and motion comparisons remain exact.

### Aerodynamic velocity response and stiff damping

The complete native 0x9f41c response stage now advances NED velocity and roll/pitch rates, updates or retains sideslip at the original forward-speed threshold, distinguishes rolling rudder response, reprojects ground/body velocity and preserves the Realistic model's below-stall ground constraint. The original velocity time-warp gate at 20 km and autopilot exclusion remain separate from the later airborne positional warp. The stiff roll/pitch fallbacks preserve their aerodynamic derivatives, control/engine contributions, previous-pitch-rate store and stick-pressure adjustment. Yaw retains the original positive/negative ten-degree cap and the stored approximate inverse-degree constant.

All 8,192 complete original updates agree, including 2,705 roll and 2,952 pitch damping fallbacks. The original startup environment executes. FPATAN and the bounded FSIN/FCOS instructions use actual host x87 instructions instead of Unicorn's binary64 approximations; all surrounding original response code executes. Coefficients, controls, loads, basis and prior state are explicit stage inputs. There are 123 compact captures. This stage has not yet been integrated into an initialized advanced-model session.

    python3 tools/check_aerodynamic_response.py

### Aerodynamic air data and atmosphere integration

The native 0x98ebc..0x991cc prefix refreshes the physical double basis, projects both ground and wind-relative velocities into local forward/right/down, retains track and angle of attack below their separate movement thresholds, and computes altitude, atmosphere, mass, true airspeed, fixed speed, Mach, pressure and elapsed simulation time. Its square-root argument and all original double/float storage boundaries remain explicit. The same atmosphere stage is used by this connected prefix and its isolated checks.

All 8,192 original-prefix cases agree, including arbitrary rotations, retained angles, signed zero, cached wind, all temperature choices, altitude boundaries and integer-indefinite conversion for excessive indicated speed. Original startup, normalization, projection, angle, square-root and full atmosphere routines execute. FPATAN uses the hardware adapter, and the capture ends explicitly before the next separately owned Mach-coefficient/fuel/failure stages; wind generation is supplied at its cached-value boundary. There are 123 compact captures.

    python3 tools/check_aerodynamic_airdata.py

The complete response comparison also passes in Release and ASan/UBSan, with all 8,192 cases and both stiff-damping branches exercised.

Both the complete response and connected air-data comparisons pass in development, Release and ASan/UBSan. Their two stage-fixture tests pass in each relevant configured build.

### Mach-dependent lift, drag and control derivatives

Native 0x99bc0 now supplies the transonic drag transition and the three distinct aircraft lift curves, including the F-22's ordered piecewise bounds and the Eurofighter's cap. The aircraft's additional lifting area remains a separate coefficient. Static pitch stability and damping use their original lift-plus-drag terms; elevator-moment, elevator-lift, roll and aileron derivatives use the original lift-only term. These distinctions are preserved rather than merged into a generic approximation.

All 8,192 complete original coefficient updates agree across all three aircraft, exact and adjacent transonic thresholds, piecewise-curve changes and supplied profile coefficients. Original startup precision and the complete routine execute; bounded FSIN/FCOS use the hardware adapter. There are 123 compact captures.

    python3 tools/check_aerodynamic_coefficients.py

Tracing the following thrust producer (0x9cc54) identifies 0x147d8/0x147c0 as engine forces/moments, distinct from the ground loads at 0x14660/0x14648. Those fields now carry their correct names in the shared force stage. The response context likewise names 0x14878 as computed engine thrust, separately from the throttle request. Force and response comparisons remain exact after these naming corrections.

The Mach-coefficient and complete response comparisons repeat exactly in Release and ASan/UBSan.

### Owned aerodynamic powerplant, fuel and engine faults

One powerplant owner now retains throttle request, computed thrust, both engines' delivery factors and fault states, F-22 nozzle angles, engine loads, fuel, flow and afterburner/readout state. Native 0x9cc54 computes dry/reheat thrust, altitude loss, engine availability and the F-22's separate nozzle forces and moments. Native 0x99424 retains dry/reheat fuel rates, idle-flow floor, fuel-specific warp/use scaling, wrapped whole-fuel readout, exhaustion and ordered afterburner-stop requests. Native 0x995e0 preserves gradual intermittent-engine decay, per-engine random sampling and the transition to failure.

All 8,192 complete original cases agree, with the routines exercised separately and in their original frame order. This includes 847 exact ordered afterburner-stop request pairs. Original startup, random generation, fuel-stop handler and numerical routines execute; bounded FSIN/FCOS use hardware instructions, while sound/model delivery are explicit intercepted output boundaries. Retained untouched load axes, native random words and all double/readout bits are observed. There are 123 compact captures.

    python3 tools/check_aerodynamic_powerplant.py

All 66 CTests and the complete powerplant comparison pass in development, Release and ASan/UBSan.

### Aerodynamic surface forces, stall and moments

The five connected native surface stages (0x9c7f0), and their simplified lift/drag/projection sequence, now compute flap lift offsets and limits, automatic flap extension, aircraft-specific stall, failed-flap behaviour, induced/gear/elevator/air-brake/chute drag, pitch and roll moments, and body-force projection. The original additional elevator-lift term is distinguished from the physical flap terms. Simplified updates retain the original cached moment terms and untouched body axes.

All 8,192 complete original updates agree, including 2,217 stalls, all three aircraft, full and simplified sequences, failed/intermittent/healthy flaps, brakes, gear, chute and supplied control/geometry/derivative combinations. Original startup and the complete numerical stages execute; bounded FSIN/FCOS use the hardware adapter. All retained scalar state, forces, moments and stall flags are observed. There are 123 compact captures.

    python3 tools/check_aerodynamic_surfaces.py

The surface and Mach-coefficient comparisons also pass exactly in Release and ASan/UBSan. The 0x141d0 derivative is now named elevator lift, following its actual 0x148a0 elevator consumer; physical flap derivatives remain separate.

### Advanced-model device input, throttle and autopilot targets

The complete native 0x9a104 handler now retains each device's distinct control targets and limiting rules: keyboard pitch/roll accumulation, Alt/ground rudder behaviour and speed gating, centered mouse dead zones/cubic response, joystick pitch/roll scaling, and mutable mouse limits. The native mouse branch updates a rudder key target without copying it to actual rudder; that retained-state distinction is preserved. Throttle keeps its per-update increments, engine-off and afterburner rules. Autopilot target altitude, heading and speed retain their original mode gates, bounds, priority and word wrapping.

Input source and direction keys now live in a shared input header rather than the Simple model's header. Device acquisition remains outside numerical input. The powerplant owns its afterburner-stop invariant, shared by fuel exhaustion and input-driven shutdown, while output delivery remains separate.

All 8,192 complete original input/control/throttle/target updates agree, including every source, device threshold edges, opposing keys, Alt, ground/speed gates, engine states and autopilot modes. They include 382 exact ordered afterburner-stop request pairs. Original startup and the full numerical handler execute; mouse range/reset/acquisition and sound/model delivery are explicit boundaries. There are 123 compact captures. The complete powerplant comparison still agrees in all 8,192 cases after sharing the stop operation.

    python3 tools/check_aerodynamic_controls.py

All 68 CTests, complete advanced-input cases and complete powerplant cases pass in development, Release and ASan/UBSan.

### Automatic/overspeed and recovery thrust controllers

Native 0x9aa4c and 0x9ab78 retain their separate speed-control strategies: force-compensated throttle changes, their distinct throttle bounds, overspeed air brakes, temporary recovery speed targets and conditional afterburner stage changes. The powerplant owns increase/decrease invariants as well as full afterburner shutdown. First-stage starts, intermediate changes and final stops remain distinct output events, preserving the original sound/model/device request order.

All 8,192 complete original controller updates agree, including 446 exact ordered afterburner changes. Both controller routines and the original afterburner handlers execute under original startup settings; only sound, SSD and optional external-device output delivery are intercepted. State, target restoration, float throttle, indicators and selected event semantics are observed. There are 123 compact captures. The complete powerplant comparison still agrees after adding these operations.

    python3 tools/check_aerodynamic_speed.py

Both speed-controller and complete advanced-input comparisons repeat exactly in Release and ASan/UBSan.

### Complete ordinary advanced-model control law

The manual path of 0x9ac9c now retains bank/stick-pressure compensation, source-dependent trim, military pitch-rate correction, flap demand, aircraft-specific stall/overspeed handling, positive/negative G deflection limits, damage-induced control movement and F-22 nozzle mixing. Overspeed uses the original temporary speed target and restores the retained target afterward. Nozzle comparison bounds and assigned full-angle clamps retain the native distinction. Autopilot route/attitude control is a separate prefix and is not claimed here.

All 8,192 complete original ordinary/manual cases agree across all aircraft and input sources, military/Realistic choices, neutral/held control targets, stick pressure, flap failures, speed thresholds, damage, engine states and nozzle mixing. This includes 47 ordered afterburner changes. The complete original 0x9ac9c routine executes with autopilot disabled, including original overspeed and reheat handlers. FPATAN, FSIN/FCOS and bounded FPTAN use the hardware adapter; only sound/model delivery is intercepted. There are 123 compact captures.

Preset inspection identifies 0x8cd0 as aircraft-specific elevator travel: the Eurofighter uses -0.8726638888888888 radians, while F-117/F-22 use 0.10471966666666664. The compiled pre-preset value 0.2 is not an aircraft setting. Surface forces, stiff pitch response and the manual law now receive this value explicitly through the aircraft parameters. Their 8,192-case comparisons are regenerated with the actual travel values and remain exact. The response corpus includes 2,705 roll and 2,940 pitch damping fallbacks with these inputs.

    python3 tools/check_aerodynamic_manual.py

All 70 CTests pass in development, Release and ASan/UBSan. Complete manual-law, surface-force and response comparisons also pass in each build with the aircraft-specific elevator travel supplied.

### Decoded complete aerodynamic aircraft profiles

The selected executable's original preset data now supplies named surface geometry/limits, Mach coefficient bases, powerplant parameters, mass/store moments, mounts, G limits, cockpit offsets, stall/reference speeds and cruise Mach. Direct literal assignments and their repeated identical writes remain data reads; computed angles, drag terms and areas use explicitly translated formulas and the original constant operands. No original instructions execute inside the game. Original formulas and area-normalization literals are retained, including the F-22 gear-drag calculation's 50-square-metre normalization rather than its 78-square-metre wing area.

The aircraft owner computes its initial pitch-stability correction and Mach-dependent damping from the original initializer equations. The retained prior flap-lift offset is an explicit input, rather than an accidental global dependency. All three complete decoded profiles agree word for word with the full original 0x95f3c initialization in fresh images. Another 384 full-initializer calibration cases agree with varied preceding flap offsets. Original startup, preset, mass/store setup and initialization execute; only FPTAN uses the hardware adapter. The compact asset-free suite retains 55 calibration captures; the complete profile check reads the user's selected reference executable.

    python3 tools/check_aerodynamic_airframes.py

### Advanced wheel/body ground contact

Native 0x9d734 evaluates gear and body contact points in physical North/East/Down space. The translation retains terrain-height projection, angular point velocity, spring/damper reactions, lateral and rolling friction, speed/weather-dependent wheel braking, progressive gear/brake failure, body-contact damage and rough-ground random bumps. Working caches remain explicit because successive contact points share them. The original force/torque accumulation order, early returns, cleared yaw rate and one-point delay between brake failure and changed braking force are preserved.

All 8,192 complete original contact-point cases agree, including 2,028 reactions, 851 random bumps and 109 failed-gear impact notification requests. Original startup, arithmetic, native square roots and random generator execute without numerical interception. Only the failed-gear output notification is intercepted and its request is observed. There are 123 compact asset-free captures.

    python3 tools/check_aerodynamic_contact.py

### Complete military-spec ground pass and carrier arresting gear

Native 0x9ce5c now combines all six original aircraft contact points, preserving shared point caches, random order, gear failure timing and the retained yaw moment at entry. Ground start assistance, cable engagement/stretch/damping, cable release/stopping, landed state, rollout distance and collision load clearing follow the original. The load threshold requests the original cockpit-bump callback; it is separate from gear damage and collision.

All 8,192 complete original updates agree: 1,537 contact passes, 401 random bump sequences, 423 gear impact requests, 1,186 arrestor transitions and 1,563 cockpit bump requests. The complete ground routine and all six original point evaluations execute; only failed-gear output notification delivery is intercepted and counted. There are 123 compact captures. The checked aircraft-profile decoder also supplies all six contact offsets, hook geometry, suspension, friction/brake limits and carrier constants directly from preset data.

    python3 tools/check_aerodynamic_ground.py

### Complete Realistic-mode ground handling

Native 0x9e0f4 is translated separately from military-spec suspension. Its three projection-only wheel checks, touchdown/liftoff gates, keyboard-elevator inhibition, weight support, distinct rough-ground and braking laws, progressive failures and kinematic roll/pitch/height corrections are retained. Carrier cable code is shared between the two translated paths; Realistic omits physical hook moments, matching the native branch. Collision does not silently acquire the military branch's load-clearing behavior.

All 8,192 complete original Realistic ground updates agree, including 1,042 random bump sequences, 830 failed-gear notifications and 1,317 arrestor transitions. The full routine and original wheel projections execute. FPATAN/FSIN/FCOS use the hardware adapter; failed-gear output is the only other interception and is counted. All physical position/velocity changes, control commands, point caches, damage, random seeds and output requests are observed. There are 123 compact captures. The complete military ground comparison repeats after sharing its checked carrier implementation.

    python3 tools/check_aerodynamic_realistic_ground.py

### Realistic attitude commands

The numerical suffix of 0x99ecc (0x99f6f..0x9a100) now converts the completed manual controls, aerodynamic derivatives and thrust-vector moments into Realistic-mode roll-rate/angle-of-attack commands. The low-speed path retains the previous roll command and copies the current attack angle. Stick-pressure correction and the original explicitly zero pitch-rate term remain. The constant 20-degree vector angle is precomputed with binary64 intermediates, matching the original 53-bit arithmetic instead of allowing compile-time extended-precision folding.

All 8,192 bounded original suffix cases agree. A declared synthetic caller frame supplies only the enclosing saved EBP/return, and FSIN/FCOS use the hardware adapter. Input/manual control and derivative production are stage inputs, rather than silently stubbed original calls. There are 123 compact captures.

    python3 tools/check_aerodynamic_commands.py

### Physical-model initialization and model switching

The complete native 0x954fc initializer now reconstructs physical axes/pose, mass, starting body/NED velocities, grounded motion resets, cold-start fuel/engine state, track heading and the initial timestep. Both store recalculations retain their original order relative to the initial mass sum. Pose conversion keeps this routine's original cyclic cockpit-offset component order. The aircraft owns application of the decoded carrier setup, keeping the shared wing area consistent across coefficient/surface consumers.

All 8,192 complete original initializations agree, including 4,095 ground starts and 143 special carrier starts. All 27 original store definitions are sampled in a mounted slot with varied offsets, prior store mass and fuel. The native initializer, store accumulation and carrier tuning execute without interception. There are 123 compact captures. The complete preset comparison also checks all decoded carrier values and their applied surface/coefficient/contact fields.

    python3 tools/check_aerodynamic_start.py

### Advanced wind initialization and updates

Native wind initialization and complete 0x996c0 now retain surface-speed smoothing, direction/vertical modulation, gust renewal, weather/terrain boosts and upper-altitude wind blending. The original initializer's five parameter profiles come directly from executable tables. Its three random draws remain ordered, including a draw stored in a legacy field with no reader. Unwritten retained values remain explicit. Gust renewal resets the same clock that aerodynamic air data later advances; the connected caller must preserve that shared ownership.

All 512 complete original initialization cases and 8,192 complete original wind updates agree, including 5,805 gust renewals and every random-state/output-float bit. The selected weather is supplied just before the original initialization block, allowing all five table entries to be checked even though retail initialization normally selects zero. Original instructions and random generation execute; FPATAN/FSIN/FCOS/FPTAN use the hardware adapter. There are 130 compact captures.

    python3 tools/check_aerodynamic_wind.py

### Assembled fresh advanced-flight initialization

The numerical owners now compose into one flight state, initialized through the decoded aircraft profile, original-ordered wind/store/physical setup and pitch calibration. It retains the preset's initial lift/drag/elevator coefficients, initial air/body velocity distinction, selected temperature, mode, gear/chute/hook state, carrier setup timing, engine state and ordered random consumption. Cockpit/device/campaign setup remains caller-owned. This is fresh initialization; it does not yet claim warm mission-to-mission retention of every native global.

The full original 0x95f3c executes in 768 controlled scene/cockpit setups. All 282 observed fields agree exactly across three aircraft, both advanced model settings, gear, mounted stores, temperature, cold/ground/carrier starts and random seeds. Only the explicit hardware transcendental adapters replace emulator arithmetic. The comparison also covers control gains and complete view position/angle/orientation exports. Reference diagnostics group fields by C++ owner and identify the native offset of a difference. The decoded preset check also covers atmosphere/control constants and the preset's starting coefficient values.

    python3 tools/check_aerodynamic_initialisation.py

The assembled state is not yet a selectable live flight model: connected multi-tick callback comparison, advanced autopilot/recovery and live host bindings remain required.


### Connected ordinary advanced-model trajectories

The owning state now executes the ordinary/manual callbacks in original order: wind/air data, coefficients, fuel/faults, input/manual laws, Realistic attitude commands where selected, surfaces/thrust, the appropriate ground path, forces/inertia, response, motion and view export. Source-specific control gains and Realistic ground-input overrides are explicit. Shared wind/time and ground/pitch-rate caches are transferred at their native boundaries. Sound/model requests remain returned events rather than numerical dependencies.

All 9,216 persistent updates agree across 36 complete original-initialized trajectories: three aircraft, both advanced models, keyboard/joystick/mouse and airborne/ground starts. All 282 numerical/control/ground/view snapshot fields, random states and ground output requests are checked each tick. The full original 0x95f3c initializer and 0x9fe14/0x9fe50 callbacks execute. Only host device sampling, sound/model delivery and the explicit hardware transcendental adapters are boundaries; no numerical flight routines are stubbed.

This first corpus retains deployed gear, so it excludes the separate airborne automatic-recovery/autopilot producer. It uses ordinary manual control and does not claim autopilot, full automatic landing or connected live-host integration.

    python3 tools/check_aerodynamic_trajectories.py

### Advanced recovery engagement and restoration

Native 0x85a7c now retains recovery eligibility, saved control configuration, autopilot mode/enable, throttle, air-brake condition and afterburner state. Restoring recovery preserves ordered reheat-handler requests and the physical-model reset request. Engagement's sound/radio request remains an output event. The shared manual/autopilot control-limit suffix is factored without changing the complete manual law or connected manual trajectories.

All 8,192 complete original recovery toggles agree: 2,575 engagements, 1,286 restorations and 726 ordered reheat changes. Original startup, recovery and afterburner handlers execute; only sound/radio/model output delivery is intercepted and observed. There are 123 compact captures.

The original restoration loop cannot terminate if the saved afterburner state is no longer reachable, for example after fuel runs out. A bounded original execution confirms that loop; the translation diagnoses it instead of hanging. This state has no terminating original output to compare. The finite comparison corpus restricts saved reheat to reachable settings and separately checks that diagnostic.

    python3 tools/check_aerodynamic_recovery.py

### Autopilot guidance and rate-to-control conversion

Native 0x9b003..0x9bb20 now computes constrained heading/altitude/climb requests, bank limits, bank/pitch response, damping feedback, recovery behavior and speed commands. It preserves the original direct physical attitude corrections when rate steps would overshoot, including quantized-angle remainder handling. The original 80-degree bank comparison versus assigned 83-degree clamp remains. Direct corrections and normal motion share their checked angle-unit rotation helper.

All 8,192 bounded original guidance cases agree: 1,208 physical attitude corrections, 410 recovery restorations and 2,102 ordered afterburner changes. Native guidance, speed/reheat control, recovery and physical rotation helpers execute; navigation targets are declared preceding-stage inputs. The enclosing synthetic frame supplies scratch space only. Output delivery and hardware transcendental adapters are the only intercepted dependencies. There are 123 compact captures.

    python3 tools/check_aerodynamic_guidance.py

The following native conversion (0x9bb20 to the common suffix at 0x9bfa5) also agrees in 8,192 cases, including actual/keyboard elevator/aileron/rudder, trim and the cleared pitch bias. Mode 3 retains its incoming controls as in the original. All aircraft and thrust-vector contributions are covered; only FSIN uses the hardware adapter. There are another 123 compact captures.

    python3 tools/check_aerodynamic_autopilot_controls.py

These are checked autopilot stages; complete target/route selection, eligibility/dispatch and connected autopilot trajectories remain to be joined.

### Autopilot route and aircraft-following targets

The preceding native stage (0x9adae..0x9b003), route selector 0x751a4 and followed-actor decoder 0x938c4 are reconstructed. Route advancement uses one-based packed cell centres, retains target height, advances below 4,000 world units and wraps to the first point. Following mode instead reads a selected actor's Q8 position and Q16 speed, retaining its distinct height, bearing and scratch conversions. Heading-only and recovery modes preserve their original differing state updates.

All 8,192 bounded original navigation updates agree, including 357 route changes and 819 followed-aircraft targets. Original bearing, route and square-root helpers execute without any intercepted dependencies; 123 compact captures are retained.

    python3 tools/check_aerodynamic_navigation.py

### Complete advanced autopilot controller and joined callbacks

The complete 0x9ac9c controller now joins its initial low-lift speed recovery, manual/guided dispatch, navigation, guidance, control conversion and common limits/flap/nozzle suffix. Its eligibility gate tests the vertical component of the aircraft's right axis (bank), rather than pitch. The manual fallback also retains the original autopilot-specific stick-pressure rule. Shared ground/pitch-rate/roll-angle scratch transfers occur at their native call boundaries.

Both advanced callbacks now consume autopilot targets and supplied route/followed-aircraft records. All 4,800 complete original flight updates agree across 1,200 scenes initialized by the full native initializer: three aircraft, both advanced models, all five autopilot modes, varied orientations/speeds and keyboard/joystick/mouse. Three manual warm-up updates precede each autopilot request. The comparison covers 787 guided branches, 722 speed-recovery calls, 35 recovery restorations and 373 ordered reheat changes, with all 314 numerical/control/navigation/ground/view/recovery fields and output events checked. Only host input/output delivery and the declared hardware transcendental adapters are execution boundaries; branch-entry hooks observe instructions without replacing them.

    python3 tools/check_aerodynamic_autopilot.py

This first corpus ends after the autopilot update. The following persistent corpus additionally covers automatic recovery and model-transition resets; automatic landing and live-host selection remain open.

### Automatic descent recovery and persistent advanced flight

Native 0x991e9..0x992f8 predicts crossing zero physical height from downward velocity when gear is retracted. It preserves the original five/seven/eight-second warning/recovery boundaries, eligibility guards and gear/speed distinction. It does not query terrain height. All 8,192 bounded original prediction cases agree, including 25 engagements and 201 warnings, with exact threshold-neighbour cases and 123 compact captures.

    python3 tools/check_aerodynamic_recovery_trigger.py

Both complete advanced callbacks now include this producer and their entry-time physical model reset. The owning flight state retains its mounted stores for the original reset-time load recalculation. A reset preserves the original retained fields, uses the existing view/frame, and replaces the current update's timestep with 0.01 seconds as native 0x954fc does.

The outer game caller (0x8f98c..0x8f996) acknowledges the current model after the callback returns. Thus a recovery request made by a key before the callback triggers its entry reset; a request raised inside the callback is acknowledged afterward and does not trigger a later reset. The callback returns its marker and events, and its outer caller must perform that acknowledgement. This corrects the earlier assumption that every recovery request resets the following frame.

All 11,520 persistent original updates agree across 120 complete-initialized 96-frame scenes: 7,819 guided updates, 6,452 speed-recovery calls, 26 automatic recovery engagements, 19 restorations, 240 entry resets, 36 descent warnings and 262 reheat changes. The full original initializer, callbacks and numerical dependencies execute; original outer-marker acknowledgement occurs between updates. Every one of the 314 snapshot fields and returned output events agrees. Gear-deployed/retracted starts, all aircraft/models/modes, varied attitudes and key-triggered transitions are included.

    python3 tools/check_aerodynamic_autopilot.py --persistent

### ILS aerodynamic approach references

The complete 0x99a30 routine supplies reference flap fraction, lift, angle of attack and current flight-path angle while ILS is enabled. Its authored seven-degree reference and flap-failure/Nighthawk rules are retained. All 8,192 complete original reference updates agree, including 2,048 retained flight-path cases and 123 compact captures. Hardware FCOS/FPATAN adapters are the only intercepted numerical dependencies.

    python3 tools/check_aerodynamic_approach_reference.py

This reference calculation is joined at its original boundary after coefficients/fuel/faults and before automatic recovery and input. Toggling ILS on and off during all 11,520 persistent advanced-flight updates also agrees, now checking 319 numerical/navigation/approach/ground/view/recovery fields and output events. This calculation supplies reference values; the separate scripted automatic-landing callback remains to be reconstructed.

    python3 tools/check_aerodynamic_autopilot.py --persistent --ils

### Scripted automatic approach, flare and rollout

Native 0x9fecf..0xa028f is a separate kinematic landing path. It rebuilds the physical frame from runway heading and scripted pitch, follows the authored seven-degree approach, raises pitch near touchdown, clamps to runway/carrier height and reduces speed by an authored per-update amount during rollout. Its destination/player integers are metres in East/Down/North order, rather than the ordinary autopilot's world units.

All 8,192 bounded original updates agree: 2,088 new flare transitions and 3,325 requests to enter the separate touchdown finalizer. Original angle helpers, integer conversions, square root, basis and motion calculations execute; hardware FSIN/FCOS adapters are the only numerical boundaries. Both return paths are observed before the separate finalizer/gear/view tail. There are 123 compact captures. Setup, finalization and the complete automatic-landing callback are still being joined.

    python3 tools/check_automatic_approach.py

### Complete scripted automatic landing

Native 0x9feb4 now includes setup 0xa0494, the approach/flare/rollout stage, touchdown finalizer 0xa02c8, original gear switching and final view export. Setup stops reheat, aligns to the selected runway/carrier, positions the aircraft on approach and returns the original 10,000-point charge. Touchdown retains the original engine/velocity, suspension-height, timestep and prior-view conversion behavior. Ordered reheat/gear/stopped output requests are returned separately.

The setup can raise deployed gear and lower it again in the same update; both original requests are retained. Original shared navigation scratch also changes coordinate units during landing: the landing owner uses explicitly named metre coordinates, then transfers retained scratch at the callback handover. Ordinary route guidance rewrites those coordinates in world units. Native angle/basis, caches and rest-of-flight state remain checked rather than assuming the landing path leaves them untouched.

All 1,200 full original callbacks agree across three aircraft, both advanced profiles, both entry phases, carrier/runway selection, gear faults, eligibility guards, varied frames and rollout boundaries: 600 setups, 160 completed rollouts and 819 ordered gear changes. All 324 flight/autopilot/landing fields, score differences and output events agree. A further 48 complete-initialized sequences agree through 15,813 active callbacks, including 24 setups, 19 complete rollouts and 34 gear changes. After a sequence becomes inactive, the capture caller stops invoking its landing callback, as the game dispatcher does.

The full original initializer and every numerical landing helper execute. The only boundaries are caller-owned score-variable lookup, sound/model delivery and explicit hardware transcendental adapters; branch hooks merely observe execution.

    python3 tools/check_automatic_landing.py
    python3 tools/check_automatic_landing.py --persistent

The automatic-landing callback is reconstructed; Alt+L key selection, its view/weapon/ILS requests and live flight-model integration are the next caller connections.

### Advanced flight key handlers and physical-reset ownership

The original 6/5/7/8/9/0/A handlers are reconstructed: cruise-speed reset, auto stick pressure, waypoint/heading/track/autothrottle selection and autopilot enable/disable. Mode selection and engagement retain their different altitude/heading capture rules, including float-world-height conversion and the Realistic track-heading override. Normal toggles save/restore their own air-brake condition; recovery uses its separate saved state.

A normal A toggle calls physical reset 0x954fc immediately. Recovery-mode A instead restores recovery and leaves its transition marker for the enclosing callback. Physical reset is now an operation of the owning aerodynamic flight state, shared by immediate key toggles and entry-time callback resets. The key result reports the immediate reset and its current 0.01-second timestep; the outer timer/dispatch order determines when that value is subsequently replaced. The external named `autopilo` on/off callback and radio requests remain returned output events.

All 1,536 complete original key updates agree across initialized aircraft, both advanced models, enabled/disabled/unknown/recovery modes, guard conditions, stored air brakes, captures and speed presets. The comparison includes 74 immediate physical resets, 16 recovery restorations, 74 external on/off requests and 698 notices, with all 316 flight/control fields and output events checked. The full original key, reset, recovery and afterburner routines execute; only external delivery and explicit hardware transcendental adapters are boundaries.

    python3 tools/check_aerodynamic_pilot_keys.py

### Complete advanced player launch and first placement

The launch owner now joins original player setup 0x8c184, its full 0x95f3c initializer, engine priming 0x8c318, route-selection initialization and the first-placement prefix of 0x8f56c. It retains authored input angles and integer coordinates separately from quantized matrices/float stores, primes the original airborne speed/throttle, applies distinct runway/carrier placement, performs the initial physical rebuild and clears the cold-start flag at its original boundary. The first ordinary callback still sees the pending prior-model marker and performs its own entry reset.

All 768 complete original launches agree across all 316 flight/control fields and ordered random states: all aircraft, both advanced models, airborne/runway/carrier starts, raw angles, mounted stores, route sizes, temperature and wind inputs. Original setup, initializer, route and placement/physical-reset helpers execute. Only host mouse setup, supplied start-surface classification and explicit hardware transcendental adapters are boundaries. Instrument/system startup conditions use their separately verified native inputs.

    python3 tools/check_aerodynamic_launch.py

### Configured mode versus active player callback

The original dispatch (0x8f8e2..0x8f98c) distinguishes the configured mode from the callback selected for an update. Military Spec and Realistic use their corresponding callbacks. Arcade/Simple with autopilot enabled instead use the military callback at or below the authored four-tick threshold, and the Realistic callback above it. Disabled-aircraft, active-pilot and automatic-landing states take precedence; an active ejected pilot clears autopilot and automatic landing. Unknown mode slots retain their original no-update path.

All 8,192 dispatch-only cases agree across every callback outcome, duration/NaN comparison boundaries and pilot-state flag clearing. Numerical callback entries are observer boundaries in this selection-only capture; those callbacks have their separate complete execution comparisons. There are 123 compact captures.

    python3 tools/check_player_dispatch.py

The aerodynamic owner now uses the canonical native configured-mode enum, while its tick context can select the callback separately. Configuration-dependent manual/guidance laws, Realistic ground-input overrides and response guards remain distinct from callback-dependent commands, ground model, load moments and motion integration. Gear-speed damage is a non-Arcade rule, including Simple's aerodynamic autopilot path.

All 23,040 complete original updates agree across 240 initialized 96-frame scenes under all four configured modes, including both aerodynamic callbacks under Arcade/Simple: 14,626 guided updates, 12,068 speed recoveries, 73 automatic engagements, 35 restorations, 480 entry resets, 37 warnings and 532 reheat changes. All 319 snapshot fields and returned output events are checked. Complete startup/first-placement comparison also agrees in 1,536 setups across all configured modes, retaining the basic-mode carrier-placement distinction.

    python3 tools/check_aerodynamic_autopilot.py --persistent --ils --all-models
    python3 tools/check_aerodynamic_launch.py --all-models

### Outer player-frame state and callback handover

The complete original tail at `0x8f98c..0x8fac4` now agrees in 8,192 cases,
including terrain collision, engine status refresh, signed fuel smoothing,
measured movement and acknowledgement of the model-reset marker. The capture
executes the native movement norm without replacing numerical dependencies.
Integer coordinates and physical NED position deliberately remain separate from
its final floating-height clamp. All 86 asset-free tests pass in RelWithDebInfo,
Release and ASan/UBSan builds.

Recovery transitions are retained in order, including engagement and restoration
within one frame. The persistent four-setting comparison checks every transition
across 23,040 updates and all 319 retained fields. Automatic landing explicitly
reports its clearing of the separate basic Mach and reported-speed caches; 1,200 complete
original callbacks check that signal alongside all 324 flight/landing fields.
Shared-state adapters name the globals shared by basic and physical callbacks,
while preserving their distinct speed, altitude and control caches.

### Connected original player loop and four live flight models

The windowed game now selects Arcade, Simple, Military-spec and Realistic
callbacks through the original outer player loop. One player owns the retained
basic and physical caches, automatic landing and ejected pilot. Shared globals
are transferred explicitly, including the original reuse of Simple landing
scratch as aerodynamic angle-of-attack/approach data. Basic speed caches remain
separate during temporary autopilot flight. Original timing supplies physical
seconds independently from world ticks; advanced callbacks retain both original
engine-fault updates per frame.

`check_player_flight.py` executes full native startup and the complete original
`0x8f56c` callback. **4,608 frames in 48 initialized scenes agree across all 324
retained fields**, including repeated manual/autopilot handovers, keyboard and
joystick input laws, refuelling, intermittent/failed engines, disabled aircraft,
automatic-landing entry and score changes. Timers, device acquisition, supplied
surface lookup and external output delivery are explicit boundaries. The actual
numerical callbacks, physical resets, navigation, recovery, touchdown and final
movement/fuel bookkeeping execute without numerical substitutes. Hardware x87
transcendental adapters retain the original instruction results.

A/5/6/7/8/9/0, recovery and Alt+L are connected to live controls. Autopilot and
flight-control MFDs consume live targets, flaps, nozzles and engine factors.
Native startup defaults to automatic stick pressure. ILS selection controls now
apply their returned input gains; Alt+L uses direct runway selection even when
ILS is already enabled. Gear may be operated on the ground, matching its native
key handler. Existing private-window weapons/MFD/result and ILS checks pass.
The complete Alt+L key wrapper and output/audio delivery still need their own
caller-level validation; the underlying landing callback has independent full
original-execution comparisons.

The complete player-loop comparison and all 86 asset-free tests pass in
RelWithDebInfo, Release and ASan/UBSan builds. All **128 retail missions** also
advance **1,024 world steps in each of the four modes** and render their resulting
scene. Private X11/GDB checks in both advanced modes observe live Down, Alt+Left
and hook input, authored seat release, pilot launch/parachute stages and the
original recovery/FINISH outcome. These checks observe state; they do not alter
the game under the debugger.

### Original configuration readers

The portable configuration owner names the original control, model, detail,
language, sound-selection, joystick-scale, HUD and realism fields. The byte and
word storage rules are retained, including the file's distinct MODEL index order.
**2,239 complete native dispatch/reader cases agree** for keyword case, signed
conversion and wrap, partial REALISM lists and floating scale input. Both original
CRT initialization routines run, including `0xde547` for floating-point scanf;
no numeric conversion or scanf routine is replaced by the capture.

Another **137 complete original file reads agree**, with CRLF, repeated keys,
missing final newline, old six-value REALISM lists, coupled Sound Blaster music
selection and the exact 1,024-byte fallback boundary. Filesystem delivery and
sound-device setup are explicit boundaries. DOS port settings and analogue
calibration remain separate from portable simulation settings. Compact captures
add two asset-free CTests. Connecting these values to startup and flight is the
next consumer integration.

### Connected configuration and desktop input

Startup now reads the original configuration and preserves command-line model,
control and language overrides. Flight consumes its HUD intensity, collision,
gear-damage, precipitation, wind-enable and recovery settings. One application
owner retains preferences across mission calls; flight options borrow that owner.
Weather-dependent wind/temperature selection and G-effect production remain
separate caller work.

Digital joystick input enters after DOS resistance calibration. The native
signed-square JOYSCALE response, signed-word conversion and +/-512 limits agree
in **8,192 complete original axis/button updates**. The complete button dispatcher
executes both sixteen-entry repeat tables; all nine command outcomes, button
combinations, repeat periods and word wrap agree. The standard first-four-button
mapping is fire, ground weapons, air weapons and target cycling. The retained
Flightstick profile also has its original view combinations. Extra analogue
hat/throttle inputs are excluded from this button-only capture. Devices can be
selected with `--joystick`; unavailable hardware falls back to keyboard input.
Joystick firing occurs before physics; held keyboard cannon fire remains after
physics, as in the original outer input loop.

The advanced mouse adapter retains its virtual stick and relative-motion word
arithmetic. **8,192 native range/accumulation updates agree**, including the
original asymmetric clamp passes. Each input adapter has 256 consecutive compact
captures in CTest. Original game callbacks remain separate from device polling;
no joystick menu-navigation feature is introduced.

Private X11/GDB tests use a temporary configuration and real keyboard/mouse
input to observe configuration-selected Realistic/keyboard flight, overriding it
with Military-spec/mouse, autopilot engagement/disengagement, radio replay and
ILS followed by Alt+L entering scripted landing. No game state is modified by the
debugger, and original installation files are never rewritten. The tests also
caught and fixed a consumed-L edge: replay and Alt+L now share one observed key
press instead of polling the edge twice. Existing UI checks explicitly select
their intended control/model so connected workstation controllers cannot change
the test, while expected HUD colours come from the original configuration reader.

All **90 asset-free tests**, both 8,192-case input comparisons and their compact
captures pass in development, Release and ASan/UBSan builds. Live window checks
also confirm the actual configuration's HUD intensity and ILS behaviour. Joystick
tests use the original repeat tables; UI keyboard tests explicitly override
controller selection so unrelated connected devices cannot interfere.


### Connected G-force redout and blackout

The original `0x73654` controller now produces live palette effects from the
player's shared G-load. Reduced realism uses the original -5/+10 limits;
realistic effects use -4/+9. Signed strain, asymmetric recovery rates, clamping
and retained colour levels agree in **8,192 complete original updates**, including
persistent sequences and floating boundary/unordered values. No dependency is
intercepted. A compact 256-update sequence is checked by CTest.

Flight updates exposure once per simulation update; paused redraws do not advance
it. Mission restart and disabling G effects clear the controller. The existing
independently verified palette painter consumes its redout/blackout mode and
levels. Audio requests remain deferred with the sound integration. Weather flash
production remains separate; it is not assumed to be damage feedback.
The G-effect comparison and all **91 asset-free tests** pass in development,
Release and ASan/UBSan builds.


### Authored weather inputs to flight

The environment reader now retains the native altitude-layer flag and storm bit
separately from the shape interpreter's night flag. These feed wind initialization
and every physical update; temperature realism supplies the original table value
at launch and during air-data updates. All five wind rows and twenty temperature
values remain executable data. The reference initializer explicitly selects wind
profile zero, and its temperature-column field starts at zero with no referenced
writer. Environment names are not guessed to select unused table rows. In this
reference path, temperature enabled therefore gives 29 Celsius (302 native Kelvin)
in every environment; disabled gives the original 288.15 Kelvin.

`check_weather_bindings.py` runs the complete original environment parser and
`0x95f3c` physical initialization in **1,056 cases across all 33 environments**.
Cloud/storm strength, all wind fields, temperature selection and ordered random
states agree. The profile is deliberately dirtied before initialization to check
that the native code selects zero itself. Resource bytes, allocation and fixed
clock input are supplied; no environment or weather arithmetic is replaced.
The earlier complete wind-update, atmosphere and player-loop comparisons cover
the numerical consumers. Visual cloud, precipitation and lightning rendering
remain separate work.
The weather comparison, existing 33-environment palette/binding checks and all
91 asset-free tests pass in development, Release and ASan/UBSan builds.


### Original flight preference writer

The portable writer follows complete native `0xa6808`, retaining all unrelated
configuration bytes and updating REALISM, DETAIL, JOYSCALE and HUDCOL in their
original order. It preserves first-prefix removal, case sensitivity, existing
line endings, three-column rounded sensitivity and signed HUD-word formatting.
**512 complete original file writes agree**, with only filesystem delivery and
output intercepted; native string search, formatting and each setting writer run
unchanged. 128 compact captures provide an asset-free CTest. A private-directory
integration test verifies save/reload, lowercase filenames, preserved hardware
settings and no writes for unavailable or oversized files. Unlike the original
fixed stack buffer, output cannot overflow or exceed the reader's 1,024-byte
limit. This safety boundary is explicit and excluded from numerical equivalence.


### Original dialog drawing

Native window bodies, double bevels, raised and selected buttons, flat outlines
and centred proportional labels now have portable indexed drawing. **320 full
native frame comparisons agree pixel for pixel** in development, Release and
ASan/UBSan builds. The capture executes the original form/widget painters and
raster routines with VGA writes observed; it substitutes no drawing operation.
This is the drawing foundation for the original in-flight configuration dialogs.


### Original in-flight configuration and live model changes

Backtick opens the original main, realism and quit-confirmation dialogs. Their
labels come from the selected executable, and all three page images plus each
preference selection agree in **200 full original controller/frame cases**.
Original widget construction, selection helpers, text and raster routines all
execute; device acquisition/cursor, physical page switching and heap allocation
are supplied boundaries. The original menu restricts mouse selection to Arcade,
with keyboard/joystick choices in the other models. Returning from Arcade restores
the separate startup CONTROL preference, rather than conflating it with the live
control source. Audio retains its native unavailable labels pending integration;
video-recording input remains unfinished.

Changes update the live model, controls, detail, recovery, realism and sensitivity;
closing the dialog calls the verified compatible preference writer. The original
detail caller agrees in **4,096 cases**, preserving night and unrelated shape
flags, with 128 compact cases in CTest. Menu time and partial timer polling are
discarded on resume while the timing filter is retained. Quit cancellation and
return to enrolment are connected through explicit flight results; returning to
enrolment resets the suspended parser stack and re-enters the original root
pilot-selection statement.

The complete player comparison now has an additional `--model-changes` run:
**4,608 successive original callbacks across 48 scenes agree in all 324 retained
fields**, including all four selectable models, control changes, autopilot,
recovery, faults and scripted landing. This exposed body/display matrix sharing
when crossing Simple/Arcade, Arcade's model-entry ground placement before speed
calculation, and its replacement of the previous-position snapshot after contact
adjustments. Those belong to the native caller and are now represented explicitly.

Private X11/GDB input checks observe paused simulation while the dialogs are
open, real mouse choices, live Military-spec physics after selection, saved
realism/detail/sensitivity/HUD preferences, retained hardware/unknown lines, quit
cancellation and clean resume/exit. A separate normal-front-end check restores a
DOS pilot, completes original arming, enters flight, selects Return to ENROL and
gets a pixel-identical copy of the initial enrolment screen. No original install,
user save or game state under the debugger is modified.

All **94 asset-free tests**, the ordinary and model-changing 4,608-frame player
comparisons, the 200 complete menu frames/states and 4,096 detail-flag cases pass
in development, Release and ASan/UBSan builds. The private-window checks use real
input and disposable installations; audio and video recording remain separate
integration work.

## Sampled-sound request controller

The standard Sound Blaster request controller is translated, with 8,192 persistent original-execution comparisons in development, Release and sanitizer builds. These cover signed cooldown wrapping, disabled requests, priority preemption, concatenated speech, unavailable/empty buffers and external stream completion. A 256-case fixture is retained as `audio.native_sample_scheduler`; reproduce the full comparison with `tools/check_sample_scheduler.py`. The native queue and DSP/DMA command preparation execute during the comparison; only physical device reads/writes and declared stream completion are external. PCM synthesis and live triggers remain separate work.

The sample bank now reads all 106 native slots, retaining the three unnamed slots and resolving all 103 named unsigned eight-bit PCM resources. The earlier research catalogue's missing `x10` was a classification mistake: its native hash resolves resource `5f59fe09`. Original effect words remain available for the optional QSound processor; the ordinary SB DMA path transfers PCM unchanged. All definitions and complete resource bytes compare independently, and 3,866 executed native DSP-rate settings agree in all three builds (`tools/check_sample_bank.py`). Device-independent playback preserves the quantised microsecond DAC interval, sample holds and residual timing across sample and callback boundaries; `audio.sample_clock_and_pcm` covers segmented callbacks, concatenation and completion.

### Live sampled playback

The shared PCM host now opens one 48 kHz stereo device, using the same portable miniaudio adapter as Darker. It owns the callback's sample bank/stream until the device has stopped and joined. `SOUNDFX` enables standard SB sample playback; the original sound switch suppresses subsequent requests without stopping the current buffer. The original preference writer does not change device selection. Missing output devices report their error and leave gameplay available. Developer screenshot/inspection runs do not open audio.

Live bindings cover the verified gear controls/warnings, cockpit threat warning, recovery, reheat, refuelling acquisition/concatenated connection response, ejection's repeated sample and automatic-landing requests. Refuelling's former `stop_sound` label was misleading: original `0xa4edc` requests a **normal-priority** sample; `0xa4dfc` requests an urgent one. Typed event names now preserve that distinction.

All 96 asset-free tests pass in development, Release and ASan/UBSan builds. Complete original dialog/frame comparisons additionally cover 200 supplied sound/music availability and enable states in each build, alongside the existing 200 unavailable-device cases. The native 6,144-update refuelling comparison remains unchanged. `tools/check_window_audio.py` creates private X11 and Pulse sinks, verifies live gear PCM, dual mono, off/on behavior, retained device configuration and clean shutdown; it passes in development and ASan/UBSan without playing test sounds through the user's speakers. Synthesised music/auxiliary banks, QSound processing, other ambient/combat/radio triggers and full-session audio comparisons remain separate work.

### OSL music resources

The OSL1 resource reader and packed/unpacked row decoder are translated. `tools/check_osl_patterns.py` executes the original 16-bit relocation and row decoder: all 81 resources, 1,894 complete patterns and 121,333 rows agree in every eight-byte channel cell and cursor boundary in development, Release and ASan/UBSan builds. The 128 retained synthetic cases cover all mask widths, note/effect packing flags, uncompressed rows, channel counts and empty compressed rows (which still consume one mask byte). They run without game assets as `audio.native_osl_patterns`. Device uploads are an explicit boundary of this resource-only comparison; sequencing and synthesis remain separate work.

### OSL sequencing and LAPC-I device

The native OSL playback controller and LA device callbacks are now translated. The controller owns sixteen resource/playback slots and the original 32 shared channel records; named fields retain row cells, pitch/portamento, arpeggio/retrigger state and output caches. The device owns its MIDI running status and patch-upload templates, read as external data from `START.EXE`; no original instructions execute in the game.

`tools/check_osl_la.py` compares 4,938 complete original device callbacks and 400,272 MIDI bytes, including 842 actual patch uploads. `tools/check_osl_sequencer.py` compares all 31 LA resources over 170,635 complete original ticks and 872,811 MIDI bytes at game volume 75, retaining every slot/channel field and pattern cursor. Both comparisons pass in development, Release and ASan/UBSan. A second full corpus comparison at audition volume 127 agrees on 875,157 MIDI bytes.

The joined comparison retains details that isolated patch tests cannot establish: six-byte parameter headers, signed wrapped portamento intervals, both native modulation-scaling stages and secondary chord notes accompanying retriggers. These are comparisons through the first native traversal/capture boundary, rather than complete gameplay sessions; shared timer integration, repeated resource changes and hardware PCM remain separate gates.


### Live LAPC-I music and the shared timer

The presentation interpreter now dispatches its original synthesised start/stop
requests to the live OSL player, retaining high-index preselection and native
volume 75. Munt receives the translated driver's MIDI and SysEx immediately;
CM-32L ROMs are preferred, with compatible MT-32 pairs also accepted. ROMs remain
external. No Darker conversion bank, arrangement correction or percussion
workaround is applied. The music switch sends the original zero/restored volume
and gates subsequent sequencer ticks while the synthesiser continues rendering
its existing voices and reverb. One PCM device mixes this with sampled effects.

The earlier standalone auditions followed a requested 50 Hz clock; the game
does **not** use that rate. Its launcher supplies `BX=0xffff` when initialising MEDIT, disabling that
library's private PIT programming. The shared game IRQ at master offset `0x1e03`
calls the music tick while enabled, with PIT divisor `0x5555` (21,845), about
54.62 Hz. Requested clock changes, including zero, do not reprogram or stop this
shared IRQ. `tools/check_osl_sequencer.py --host-irq` executes the complete
original shared handler and verifies both PIT writes and its game counter:
**31 LA resources, 175,459 complete ticks and 891,656 MIDI bytes** agree. This
extends the isolated comparison through the zero-clock request in `air.rld`.
The research FLACs now use this verified game clock and native song volume 75,
while remaining standalone first-traversal auditions rather than interactive
flight sessions.

Music timing retains the shared clock phase across track changes and mute/resume.
Its rational sample clock has callback-boundary tests at 8, 44.1, 48 and 96 kHz.
`music_player_probe` additionally compares complete MIDI messages and their exact
sample timestamps across varied callback sizes, mute/resume, retained selection,
resource replacement and restart. Hardware output is checked for finite, audible
PCM separately: Munt's pitch jitter uses a shared random generator, so changing
voice-processing chunks need not produce bit-identical chip output. The MIDI
stream assembler separately covers running status, real-time interruptions,
truncated messages and preserved complete SysEx/checksum errors.

The original LAPC intro filename has a trailing space. Its archive hash misses;
the native loader then falls back to the DOS loose-file path. Archive lookup now
has that same precedence and fallback, with case-insensitive DOS components and
trailing space/dot handling. Loose files also pass through RA decompression.
The asset-free loader fixture checks archive precedence, the padded intro path,
compressed loose data and rejection of parent/absolute paths. This does not
change the archive hash or silently rewrite the original filename list.

`tools/check_window_music.py` runs the actual introduction with a disposable
installation and private X11/Pulse sink. It verifies audible stereo output,
loose intro lookup/decompression and clean shutdown without routing test music
to the user's speakers. All 100 asset-free regressions and the 1,309-message/sample-timestamp player
comparison pass in development, Release and ASan/UBSan. The private-window
music and sampled-audio checks also pass under sanitizers. Other OSL devices,
auxiliary banks, in-flight selection, serial byte-level latency and complete-session
audio comparison remain open.


### Sound Blaster FM music

The original OSL FM driver now feeds the pinned DOSBox OPL chip core, using the
same standalone hardware adapter approach as Darker. Native MUSIC modes 1/3 and
`--music=soundblaster-fm` use this path; no ROMs are required. Only device timing
and output are supplied. Patches, nine-voice allocation, pitch tables/bends,
register ordering and volume behavior are translated from TFX's driver.
Nonzero channel volume is quantised to 127 by the original FM callback; its
carrier attenuation helper then replaces the patch attenuation. These quirks
are preserved rather than applying the LA driver's volume behavior to FM.

`tools/check_osl_fm.py` compares **4,409 complete callbacks, 14,218 register
writes and all 313 actual FM instrument uploads**. The complete shared-IRQ
comparison (`tools/check_osl_sequencer.py --mode adl --host-irq`) covers all
**27 FM resources, 160,813 ticks and 2,510,060 OPL writes**, retaining every
sequencer slot/channel field as well as the output stream.

An unnamed ten-channel resource exposed a further original lookup quirk: the
initial instrument index 255 aliases a relocated pattern pointer beyond the
instrument table. Its parameters therefore use the pattern's two-byte prefix,
rather than the ordinary instrument's four-byte prefix. That established alias
is represented explicitly and has an asset-free synthetic regression, without
bundling any original bytes. All 31 LA resources were rechecked after this change.
The retail filename `tunes/antiarm.adl` is genuinely absent; a failed selection
leaves music unavailable for that request, without aborting gameplay.

The whole/fragmented callback comparison agrees in **1,872,000 exact FM PCM
samples** across mute/resume, retained selection, replacement and restart.
Output is dual mono, with continuous resampling phase at 48 kHz. The private
X11/Pulse test verifies the ordinary introduction reaches the live device and
shuts down cleanly, including under ASan/UBSan. There are now 101 asset-free
regressions. In-flight selection and remaining audio consumers are still open.


### Sound Canvas synthesis and device effects

The SCC driver now joins the same live player. Its own instrument mapping,
rhythm-part selection, GS setup/checksums, note/controller handling and extended
GS/NRPN effects are translated; its generic OSL reverb callback deliberately
returns unchanged. `MUSIC=4` selects SCC-1A/CM-300 v1.30 emulation by default.
Explicit `--music=roland-scc1a` and `--music=roland-sc55` choices distinguish this
from SC-55 v1.21 and from the original SCC-1, whose complete matching ROM set is
unavailable. No General MIDI replacement bank or arrangement repair is used.

The pinned Nuked Sound Canvas hardware backend follows Darker's adapter, with
full short-message and SysEx delivery. Firmware startup and queued GS reset run
before the first song timeline. Its native PCM clock/resampling phase survives
callback boundaries. ROM identification is content-based within the selected
external directory. Driver state and the synthesiser share one owning device;
the player retains independent resource and timer ownership.

`tools/check_osl_scc.py` compares **4,347 complete callbacks, 46,744 MIDI bytes,
all 251 original instrument uploads and the 29 supported GS/NRPN commands**.
The full original shared-IRQ comparison covers all **23 SCC resources,
156,789 ticks and 700,595 MIDI bytes**, including complete channel/slot state.
The live controller test independently agrees on **1,498 MIDI messages and
exact sample timestamps** across callback sizes, mute/resume, retained selection,
replacement and restart. Firmware PCM remains finite and audible through those
transitions for both available ROM models. The private-window check verifies
stereo presentation playback and clean shutdown for SCC-1A and SC-55.

One unused native extension is explicitly unsupported: command 29 calls the
reset routine with DS still pointing at MEDIT, so its SysEx templates come from
shared playback memory instead of device data. No pattern in the 23 stored SCC
resources uses it. The translation reports that missing caller context instead
of inventing portable garbage bytes or pretending the command sends valid GS.
Other extension commands retain original details, including command 16 reusing
chorus address `0x38` and NRPN's fixed final fine-data value 17.

All 101 asset-free regressions pass in development, Release and ASan/UBSan;
complete resource/callback and live firmware comparisons are exercised separately.
The remaining audio work is the high-level flight music/effect controller,
its lifecycle/caller bindings, other sampled triggers and complete-session
comparison. Original `MUSIC=1` is the hybrid SB/CD option: its synthesised
presentation path must not be confused with CD playback during flight.


### Flight music cue and frame controllers

The high-level cue wrapper (`0xa5a9c`), both LA/FM-SCC selectors (`0xa5860`,
`0xa556c`) and non-CD frame controller (`0xa5374`/`0xa5210`) are translated into
named state, context and resource/order requests. They retain temporary-cue
return state, inhibited/collision/disabled gates, the distinction between music
enablement and background music, and Sound Blaster engine playback while music
is off. Engine order uses the original x87 truncation of `(throttle - .6) * 10`;
refresh uses a greater-than-.1 throttle change and the original `.59f` threshold.
No timer estimate is used to guess when a temporary cue has finished.

`tools/check_music_control.py` compares **8,192 complete original cue decisions**,
including 3,056 accepted plays. `tools/check_music_poll.py` compares **8,192
complete original frame updates**, including 552 selected plays, engine shutdown,
retained float bits and sequencer-driven cue returns. Original decision branches
and numerical helpers execute. Resource/filename replacement and the protected to
real-mode driver bridge are declared boundaries; playback-active state is supplied
at the actual native query. CD status/timing remains deferred. Two 256-case
asset-free fixtures retain these original results for regression testing. Both
full comparisons and all 103 asset-free regressions pass in development, Release
and ASan/UBSan builds.

These comparisons establish controller behavior, not yet its complete game
lifecycle binding. Native mission entry consumes the existing theme preselection
or defaults to bank 10; LA's profile selection and the initial cue must also be
preserved when joining it to live playback. Auxiliary resources, trigger callers,
menu stop/restart behavior and whole-session output remain integration work.

C++ module scanning is explicitly disabled for this non-module project and its
isolated hardware backends. Scanning generated DBOPL sources was unnecessary and
triggered a Ninja dynamic-dependency assertion during target regeneration.


### Live flight music lifecycle

Mission entry now consumes the original retained theme selection (or defaults
to bank 10), preserving LA's authored profile selection, including the shared
profile for theme indices 12/13. The initial cue, per-frame sequencer queries,
temporary-cue return, throttle-driven engine bank and resource replacement run
through the verified controllers. Synthesiser/timer state survives song changes.
Presentation return stops/releases the mission resource before the parser resumes.

The in-flight music switch follows the original stop/restart behavior. Engine
playback can continue with music off on the ordinary SB path. Re-enabling sends
the native volume-127 restoration, stops the current sequence and reselects the
proper arrangement at game volume 75. Its timer gate remains distinct from music
enablement. The explicit FM option selects native MUSIC mode 3, avoiding the
hybrid SB/CD option's CD branch; CD delivery remains deferred.

Ejection and ILS requests now reach this controller. Their former `voice` names
were misleading: they select music cues, rather than sampled speech or an
independent auxiliary bank. ILS also forwards its actual normal-priority sample
8 request, now named as an afterburner-stop sample rather than a generic tone.

`tools/check_music_session.py` joins the **original 32-bit game controller to
its original 16-bit OSL sequencer/device**, supplying only filename/resource I/O,
protected/real-mode ABI transfer and physical hardware boundaries. A scripted
10-second lifecycle exercises mission entry, transient cues, mute/engine state,
restart, teardown, presentation music and mission re-entry. Across 546 shared
IRQ ticks, every complete output event and exact sample timestamp agrees:
**4,039 FM register writes (nine original file loads), 1,286 LA messages (three
loads), and 1,812 SCC messages (six loads)**. Original cue branches, resource
replacement, playback operations, queries, instrument uploads and IRQ code all
execute. This is an integrated audio comparison with supplied aircraft state,
not a whole-game physics/campaign session or serial-instruction timing check.

The callback segmentation/lifecycle probe now also includes those flight phases:
**2,576 LA / 3,218 SCC messages with exact timestamps** agree, and FM agrees on
**2,832,000 complete PCM samples**. Both Roland firmware backends remain finite
and audible through the lifecycle. Private X11/Pulse tests verify audible live
mission-bank playback and clean shutdown for FM, LAPC-I and SCC-1A. The isolated
sample-window test explicitly disables music so it continues measuring sampled
sound independently of the newly audible flight tracks.

All 103 asset-free regressions, all three joined original-code transcripts
and the exact FM PCM comparison pass in development, Release and ASan/UBSan.
The independent 1,536-case ILS selection and 4,096-case ejection handoff
comparisons also remain unchanged.

Remaining audio bindings include the other combat/result/weapon triggers,
Gravis's separate auxiliary resource path, QSound processing and broader
whole-session coverage. These results do not close those gates.


### Mission-result and confirmed-hit audio

Mission completion now emits an ordered typed event between prior target notices
and the completion notice. Its caller sends the original music cue 3, then
urgent sample 41. Protection completion still plays that pair even when it has
no completion text. The existing fixture now retains both state and ordered
output: **4,439 complete original frames across 512 sequences agree**, including
all completion kinds, target bookkeeping, timers and termination.

Confirmed player kills now play music cue 10 and the native normal-priority
concatenation of explosion sample 12 followed by the original random choice of
sample 34 or 70. The former `loss_effects::radio` was a sample descriptor index,
not a text message; its name now reflects that meaning. All **4,096 original
credit cases** additionally verify the exact music request and callback arguments,
while retaining counter, permission and random-order checks. First player loss
also forwards cue 1; its association with the existing first-loss event is
verified across **4,096 complete original loss/impact cases**.

Named music/sample request enums keep these IDs with their intended meanings.
The joined 32/16-bit audio lifecycle comparison now also includes confirmed-hit,
failure and completion arrangements: **4,473 FM register writes, 1,446 LA
messages and 2,023 SCC messages**, with exact sample timestamps over the same
546 shared IRQ ticks. File-load counts are 12, 3 and 8 respectively. This extends
the earlier lifecycle comparison; it still supplies aircraft state and file/I/O
boundaries rather than claiming a complete simulated mission.

The extended progress/credit/loss and all three joined audio comparisons pass in
development, Release and ASan/UBSan, as do all 103 asset-free regressions. FM's
2,832,000-sample callback comparison remains exact after adding these cues.


### Incoming-projectile warnings and the hostile-aircraft lamp

The projectile population now decodes weapon-definition byte 43 and runs the
original nearby-player warning pass after each motion callback. Its signed-word
cooldown advances and its count resets on each native projectile substep. A
player-targeted projectile strictly within 2,000 integer world units requests
music cue 9 and one of urgent samples 17/28/23, selected from the shared game RNG;
the 500-tick cooldown prevents competing close missiles from consuming more
randomness. The broader 8,000-unit count has a separate end-of-pass branch:
exactly two missiles request sample 38, more than two request sample 39, and both
branches clear the count. These requests retain the original sample scheduler's
priority and retrigger handling.

The cockpit threat lamp has a different source: native `0x836ac` sets `0x763e`
when the enemy aircraft registry contributes an in-range radar contact. The live
radar scan now retains that state and forwards it to the existing lamp painter
and one-shot sample-43 latch. It does not substitute the missile counter for the
lamp flag. Actor registry membership remains separate from model classification.

`check_projectile_warnings.py` executes the complete original `0xa31cc` pass:
**8,192 cases** agree on cooldown/count, shared RNG and ordered music/sample
requests. Another **10,757 frames across 512 persistent populations** execute
its original `0x71fd4` clock wrapper and compare the live C++ runtime, including
cooldown expiry and zero-tick updates. Motion and player distance are supplied;
nearby projectile-trail emission and world contacts are declared output
boundaries. The comparison does not establish those effects or complete combat
sessions. A compact 256-case fixture runs without original assets.

The extended sensor comparison retains all prior slot/selection and seeker
checks and additionally compares the native hostile-scan flag over **1,024
persistent radar frames**. The existing **4,096 seeker states** and **3,864
projectile population frames** still agree. All **104 asset-free regressions**
and the warning/sensor original-code comparisons pass in development, Release
and ASan/UBSan builds.


### Player ECM, stealth and countermeasure controls

E/S now operate the original aircraft-specific ECM/stealth modes, and Insert /
keypad 0 and Delete / keypad decimal release player chaff and flares. The original
The F-117 ignores E/S; Eurofighter/F-22 mode changes remain mutually exclusive.
Enabling stealth clears selected/tracked/guidance targets and suppresses the
normal air-lock controller. ECM reaches the original guided-projectile decoy
logic. HUD labels and cartridge counts now read mutable flight state, initialized
by the existing native cockpit setup, rather than permanent startup values.

The native chaff/flare key gates prohibit release during ejection, collision or
aircraft disablement and spend a cartridge before requesting a decoy. Strength
is 60 for chaff, 40 for flares and 5 for flares with any afterburner stage active.
The live projectile population owns creation, diversion and shared randomness;
input does not duplicate those rules.

**8,192 complete original key-handler cases** agree on inventory, mode state,
aircraft/loss gates, target clearing and exact release arguments. A 256-case
asset-free fixture retains those checks. The expanded population comparison now
covers **5,796 frames** with all 27 weapons, AI decoys and the three player
strengths, and additionally checks both shared RNG words at every frame. The
separate **52,224 original projectile transitions** still agree, including ECM,
guidance diversion, ejection and authored stages.

A private silent X11 installation exercises real key input, observed through GDB
without altering state: player-owned decoy requests, changing HUD counts, ECM in
motion inputs and stealth in HUD/sensor inputs all pass, as does normal shutdown.
The same check passes under ASan/UBSan; leak checking remains enabled in separate
runs outside ptrace. All 105 asset-free regressions and the defensive-key/native
population comparisons pass in development, Release and ASan/UBSan builds.
These are composed controller/input checks, not a complete original combat
mission or a claim that every remaining flight shortcut is integrated.


### Airborne loss, mayday and first-impact samples

The loss transition now returns named effects from an explicit input context,
keeping first aircraft loss separate from first physical impact. An airborne
fatal loss requests normal-priority explosion sample 12, followed in the same
sample sequence by `mayday` (72) only when the world down coordinate is strictly
less than -200. A subsequent first impact requests its own normal-priority
explosion. Repeated loss/impact frames retain the original one-time behaviour,
while the existing first-loss music cue 1 and model event 21 remain ordered ahead
of these samples.

The complete native loss comparison now observes the actual variadic audio
callback, including priority, descriptor count and descriptor order; native
`0xa4f2c` executes rather than being replaced. **4,096 states** agree across
invulnerability, repeated loss/impact, exact -201/-200/-199 height boundaries,
countdown expiry and signed-word wrapping. The existing asset-free loss fixture
retains state and audio outputs together. This comparison and all **105
asset-free regressions** pass in development, Release and ASan/UBSan. Crash
model/palette classification, aftermath artwork and full loss-camera handoff
remain separate integration work.


### Land/water crash models and player-loss camera

First impact now exposes its own transition effect. The live caller selects the
original `crash` or `plop` assembly, restarts its authored stream, hides the old
aircraft and places the effect with cleared Euler angles. Native collision 2
forces the land choice; a sea-classified terrain definition, classified water
pixel or water-world flag selects the splash otherwise. The separate native
water-world field is used; renderer detail flags are not environment flags.
The old diagnostic `AIRCRAFT LOST` overlay has been removed.

First loss now queues the original impact-orbit view (25). Its target follows the
player's world-integer snapshot converted to Q8, as native `0x7d270` does; it
cannot accidentally continue following an earlier enemy kill. Both crash models
have named ownership and restart with a new mission. The existing disabled
flight controller continues to own post-collision motion.

`check_impact.py` executes **4,096 complete native `0x7da20` classifications**
with supplied projection/pixel access, covering every palette byte, projection
failure, strict screen boundaries and the water-world override. Another **4,096
complete native loss handlers** check crash selection, the absent-terrain
sentinel, terrain type, sampled surface, the collision-2 override, zeroed angles
and the 200-tick countdown start. An asset-free fixture retains all palette
bytes plus representative boundary/selector cases. The existing 4,096 loss cases
now additionally observe the first-impact transition and confirm the exact
first-loss camera-25 request alongside music/model/audio effects.

The host samples its retained indexed framebuffer through the retained view's
Q15 point projection. Original frame-page age and complete world-renderer
fidelity remain open; these supplied-input comparisons do not establish them.

Private silent X11 scenarios deliberately seed a collision through GDB, then
observe the live selector, upright effect placement and player impact-orbit
target. These are explicitly controlled integration cases, unlike observer-only
input tests. Land and open-water handoffs render and exit cleanly in development
and ASan/UBSan. All **106 asset-free regressions**, original classifier/selector
cases and the extended loss cases pass in development, Release and ASan/UBSan.
This closes the basic crash-model/camera binding; palette flashes, debris and
broader aftermath/session rendering still have separate consumers to integrate.


### Permission to fire and the remaining cockpit warning inputs

Actor tracking/firing notifications now reach the original rules-of-engagement
state, radio and sampled audio. Player tracking grants permission and refreshes
the 30-rendered-frame cockpit notice; its first text notice is suppressed for
the F-117 and for Arcade/training. The firing handler can grant permission
even when another AI aircraft is the target. Its first ordinary-mode request is
urgent sample 45 followed by the permission text; later firing requests use
normal sample 44 unless the ejection-stage gate inhibits them. These are separate
from the near-missile cue/sample/RNG controller documented above.

Air kills now pass the real permission/Arcade/training predicate into the
existing credit rules. An unauthorized enemy kill increments the native orders
counter and delivers the original persistent violation notice. Native kill
music and sample concatenation still precede that notice. Campaign exemptions
come from the running presentation parser, independently of the selected flight
model; direct diagnostic launches retain their existing Arcade-rule exemption.
The two native permission strings are identical, so the resource reader owns one
text value plus the distinct orders warning, all read from external `TFX.DAT`.

The launch/guidance lamps now consume native `0x70b4c`'s first/newest projectile
targeting the player. It has no distance, motion or spent-state filter. Selection
updates after drawing, while the next draw reads the retained pool record's
current weapon category. The pool already keeps record addresses stable across
retirement and reuse, so this deliberately preserves the native retained-pointer
behaviour without dereferencing freed C++ storage. This is a third distinct
warning source, alongside the hostile-radar lamp and permission notice.

**8,192 complete original permission handlers** agree in state, ordered
radio/sample requests, aircraft/mode/ejection gates and exact signed-word frame
timer wrapping. Every resulting authorization state also agrees with the actual
native score/violation branch. The expanded **4,096 full credit cases** observe
the exact persistent violation request as well as existing counters, RNG, kill
music and sample descriptors, with authorization supplied through either parser
flags or the native permission latch.

The actor producer now forwards nonplayer firing notifications too. All **192
retail programs / 352,896 controlled combat updates** still agree. Four new,
entirely test-authored crossfire scenarios put the player outside acquisition
range and compare **7,168 mixed-pool updates**, explicitly requiring their **17
AI-to-AI firing notifications**. These compact fixtures run without game assets.
The **5,796 projectile population frames** now also execute native `0x70b4c` and
check both its selected identity and the prior warning record's weapon after
retirement/recycling, alongside full projectile/RNG/event checks.

All **108 asset-free regressions**, permission/credit original-code comparisons
and retained-projectile comparisons pass in development, Release and ASan/UBSan.
Actor movement, notification delivery and scoring have declared comparison
boundaries; these results do not claim a complete original campaign/combat
session or close the remaining collision, ground-permission and renderer gates.


The private silent live-engagement check uses the authored airborne Arcade level
two, selected analytically from its start and hostile acquisition range. GDB
observes natural enemy tracking reaching permission state and the cockpit
notice, then a player-targeted projectile reaching the launch/guidance lamps.
No scenario state is changed. Both development and ASan/UBSan checks pass and
exit normally. The class/pixel/state comparisons above remain the detailed
proof; this window check establishes the live delivery path.

### Fatal solid contacts and the immediate explosion model

The player-contact controller now preserves the distinction between an aircraft
falling after combat damage and an immediate fatal solid contact. Original
`0x7145d..0x71535` selects `kapow`, camera 27 (unbanked chase), one normal
explosion sample and a 200-tick aftermath. It does not request the ordinary
first-loss music/camera sequence. The subsequent `0x7d270` branch sets the loss
result, decrements the signed-word countdown, and exits strictly below zero.

World contacts continue after a collision. Before querying a previously
fatal ordinary model, the controller executes its reset entry; a query that
does not destroy it again restores entry 17. This lets a falling aircraft
encounter solid scenery without confusing its earlier combat destruction with
a new impact. The no-hit branch releases the native movement hold. The other
legacy word cleared there has no identified reader and no invented gameplay
meaning. Pilot-phase gates now suppress aircraft loss/ground-impact model
changes, while preserving the original outcome and countdown branches.

**8,192 native post-contact transitions** agree on every branch, collision and
countdown write, held-motion/legacy-word clearing and ordered camera/model/sample
outputs. SSD fatal state is the supplied input; SSD execution and geometry have
separate comparisons. The expanded **8,192 complete loss-controller cases**
include immediate explosions, pilot gates, existing land/water impact handling,
invulnerability, wrapping and exact audio sequences. Compact fixtures cover the
contact branches without distributing original game data.

Private silent window scenarios explicitly seed the contact/loss inputs, then
observe the live model handoff, placement and camera for land, water and solid
impacts. They establish integration, not natural collision discovery or complete
session equivalence. The selected-refueller contact, transient collision effects
and broader aftermath rendering remain separate integration work.

All **109 asset-free regressions** and the contact/loss native comparisons pass
in development, Release and ASan/UBSan. The natural observer-only engagement
check also passes after the contact-loop change, preserving permission and
incoming-projectile warning delivery.

### Selected-tanker collisions and unauthorized terrain targets

The player's contact query now uses the querying SSD's real impact-damage word,
read before its temporary reset. The three ordinary aircraft initialize it to
51; it was previously being supplied as zero. The query retains its original
source handle even if an earlier contact switches the visible player to kapow.
After ordinary world contacts, only the selected refueller receives the extra
native `0x71535` query. A newly fatal player state restores entry 17 and sends
that tanker through `0x785ec`'s forced fall, without sampling combat ejection.
The existing combat-damage path retains its random ejection choice.

Twelve test-authored scenarios compare **98,304 persistent aircraft updates**
through that original collision-fall path, including subsequent impact and
retirement. Both original and C++ checks explicitly require unchanged shared
RNG at each forced-fall request. They do not claim a naturally reached aerial
refuelling collision; its selected-target caller and SSD geometry remain
separate verification boundaries.

Projectile contacts now skip terrain once an earlier contact has destroyed the
querying shot. Newly destroyed terrain cells drive the native ground-target
rules. Authorization uses the original unwrapped cell match against authored
mission targets, independently of radar/fire permission. Player-owned authorized
hits request the explosion sample. Unauthorized hits request cue 5 and the
normal explosion/unauthorized-target sample sequence even for Arcade or decoys.
The subsequent orders gate exempts Arcade, chaff and flare; training produces
its ordinary warning, while ordinary campaign flight increments the signed-word
orders counter and displays the persistent return-to-base warning. Text remains
read from external `TFX.DAT`. Static-target completion/credit continues through
the existing authored-target observer rather than inventing mobile-kill credit.

**8,192 complete original `0xa373c` contact passes** agree on exact target-cell
matching, absent/already destroyed/intact terrain, player/AI ownership, all 27
weapon IDs, Arcade/training gates, counter wrapping and ordered music, sample
and radio output. Geometry and air/mobile victims are supplied boundaries;
blast/debris output is captured separately. The original bookkeeping helper
reads the cleared shot's owner from DOS flat address `0x4b`; the capture supplies
zeroed low memory for that unused owner, leaving the helper's instructions
unchanged. A 512-case fixture exercises these rules without original assets.

Private silent live refuelling-control and natural-engagement checks both pass
after the contact-source and terrain-gate changes. Full transient effects,
area flak and complete combat-session equivalence remain outstanding.

All **111 asset-free regressions** and the complete terrain-credit native cases
pass in development, Release and ASan/UBSan.

### Native temporary-effect pool

The original `0x7afa4`/`0x7b144` pool owns 100 slots, each with ten independent
SSD models: hit, miss, splash, boom, clatter, cannon splash/hit/miss, crash and
fireball. Slots are consumed from the native reverse free order, appended to an
oldest-first active chain and reused from its head after the pool fills. There
is no invented expiry timer: the selected SSD's own streams stop drawing it.
Positions retain signed-word wrapping. Cannon-hit requests retain their normal
sample-14 request before model reset.

The C++ pool expresses the same allocation order with a bounded circular index
and an oldest-first range. The live host owns the 1,000 independent model
instances, resets only the selected slot/model on spawn, queues active effects
for both the main view and scene-backed MFDs, and includes them in the player's
post-terrain/mobile contact pass. Word positions are placed in the nearest world
copy for each camera. Aircraft ground impacts now request crash or splash from
the native terrain/sampled-surface choice; newly destroyed CBU-55 terrain targets
request their boom model. Other projectile/trail/flak producers remain separate
integration work.

**8,192 complete original pool operations** agree on all 1,000 model-load
bindings, ten kinds, position wrapping, reset and audio ordering, active
traversal, repeated full-pool reuse and reset. Resource/SSD/audio delivery are
explicit boundaries; the pool instructions and linked-list changes execute.
The 768-operation asset-free fixture covers multiple full-pool reuse cycles.

All **112 regressions** and the complete native pool cases pass in development,
Release and ASan/UBSan. An explicitly seeded private aircraft-impact case reaches
the live actor request, pool spawn, word placement and scene drawing in both
development and sanitizers. This checks the handoff, not complete effect-pixel
equivalence. All **128 retail missions** also start, render and run 64 integrated
steps in development; only two existing unavailable-vertex preview notes remain.
No complete mission/result claim is made by that short robustness sweep.

The preceding tanker-collision increment was additionally compared across all
**192 retail aircraft programs / 352,896 updates**, including each profile's
forced fall. The observer-only double-Escape/parachute/recovery window check
passes under sanitizers after the contact changes.

### Projectile impact visuals and sample ordering

Projectile ground contacts now bind the retained framebuffer classifier to the
native temporary-effect pool. Ordinary stores select hit, miss or splash;
cannon rounds select cannon hit, cannon miss or cannon splash instead. A valid
cannon hit requests normal sample 14 before resetting its effect model. The
separate player-owned explosion sample follows the original class and decoy
rules, including its cannon-splash distinction. Shots retire through the
existing population pass. A terrain query that destroys a shot also creates
clatter; all 33 retail TFX environments set the native bit enabling that path.

Weapon record byte 30 now has its own `impact_effects` field. It is distinct
from guidance category at byte 29. The nonsampling mode is used by chaff, flares
and cargo. The original bypasses classification there and reads the prior local
word. Adjacent `0xa373c`/`0xa31cc` calls have matching stack layouts: their
terrain-world and classifier locals alias. Four complete adjacent-call cases
establish base/upper world IDs 0/1 in that slot; both make the same non-water
sound decision. C++ starts each substep with that equivalent class and retains
subsequent classifier results explicitly, without reading uninitialized memory.
The output comparison also supplies arbitrary prior words to cover the branch
itself. Complete surrounding-frame stack residue is outside that capture;
chaff/flare suppress the sound and authored AI cargo is not player-owned.

All **27 decoded weapon definitions** agree on their independent guidance and
impact fields. **8,192 native impact-output cases** agree in gates, inherited
word, exact effect/model/sample order and shot retirement. They execute the
real effect allocator and sample helper. **8,192 full original pixel
classifiers** cover every palette byte in ordinary and cannon modes, along with
projection failure, strict screen bounds and the water-world override; the
existing **4,096 crash selectors** remain unchanged. A 512-case output fixture
and the expanded pixel fixture run without game assets.

Original `0x75030` is not a missing TFX flash/debris producer: it returns
immediately whenever native environment bit 1 is set, including all retail TFX
environments. The impact comparison executes that return rather than replacing
it with an output stub. The separate flak/lighting effects still have active
TFX consumers to restore.

All **113 regressions**, native output cases and pixel/selector comparisons pass
in development, Release and ASan/UBSan. A private controlled cannon scenario
supplies a ground position and the already-compared terrain classification,
then observes the actual runtime callback, cannon-effect spawn, scene handoff
and next-pass shot retirement. It passes in development and sanitizers; it is
explicitly a supplied-input integration case, not a naturally reached hit or a
complete pixel comparison. Original framebuffer-page age and world-renderer
fidelity remain separate boundaries.


### Authored area flak and independent effect pool

Mission flak rectangles now drive the original once-per-frame controller
(`0x7cc30`). Its inclusive altitude/region tests, cloud-layer gate, signed-word
delay and dedicated phase/scatter generator retain native wrapping and
rounding. That generator is independent of the shared combat random stream.
Its delay, seeds and night-model cycle belong to the application session:
mission initialization clears the pool and centre without resetting them.

Daytime regions use `burst`; nighttime regions cycle five `snake_*` models.
The separate 20-slot pool allocates in the original reverse/free order and
reuses the oldest slot when full, with no invented lifetime. Each slot has its
own SSD animation state. Positions retain native word storage and are
unwrapped near the camera before drawing. The original centre/range gate
includes its signed absolute-value edge case.

Close daytime bursts request the original normal-priority sample and climb
warning, with radio delivery gated by the ticker's idle flag (`0x7612`). Their
white palette flash uses level 40 for the pending frame, restoring level 50
afterward. The separate native cockpit-position shake (`0x7d7e0`/`0x7d94c`)
and the ordinary sun-glare producer remain unbound; this increment does not
claim complete flak feedback or full renderer equivalence. No combat
damage was added to this visual pool.

`check_flak.py` compares **8,192 complete original frames** over 16 persistent
day/night sequences: **2,871 spawns and 85 flash requests** agree in state,
random consumption, gates, placement, output ordering, pool reuse and draw
queues. Resource, SSD, sample, radio, shake setup and final drawing are observed
boundaries; the controller, random generator, integer rotation, allocator and
draw gate execute unchanged. The compact fixture runs without game assets.

The environment decoder now also retains the original signed cloud-layer
height/offset and upper-grid filename. All **33 native environment parses**
agree on these and their palette/status bindings; **1,056 original atmosphere
and wind initializations** still agree. Retaining an upper grid does not yet
implement its rendering.

All **114 regressions** and the native flak/environment comparisons pass in
development, Release and ASan/UBSan. Controlled private day/night window checks observe
the real controller, selected model, pool, word placement and scene handoff,
plus the daytime palette flash. They explicitly supply a small region, player
height and flak seeds; they are integration checks, not naturally reached
encounters or complete pixel comparisons. The private day/night checks pass in
both development and ASan/UBSan.


### Projectile contacts after seat release

The original projectile parent (`0xa373c`) excludes exactly three motion
callbacks before querying the player: attached, falling and accelerating. Its
accelerating alias remains eligible. Player-owned stores are excluded, and
helper `0xa3504` applies the invulnerability gate. Ejection state and pilot
phase do not add immunity; source-destroyed filtering occurs after this player
query. The live callback now follows those conditions rather than skipping
ejected players or already-destroyed querying projectiles.

Player system-damage delivery (`0x8f460`) likewise remains active after seat
release, retaining SSD damage events and shared random consumption. This does
not invent a collision volume for the pilot: each current model's original SSD
program still decides whether contact actually occurs.

**11,232 original dispatch cases** agree across every weapon and motion
callback and all combinations of ownership, invulnerability, ejection, pilot
phase and source-destroyed state. Original query setup and parent/helper gates
execute; source-handle lookup and final collision geometry are declared
boundaries, and capture stops before later actor/terrain phases. A 416-case
fixture runs without game assets. The existing **8,192 exact damage/repair
cases** now also vary the original ejection and pilot globals, confirming
unchanged faults, accumulated damage, notifications and RNG.

The flak investigation also identified its remaining callback more precisely:
`0x10be6`/`0x10be8` are cockpit screen offsets. Its short pulse shakes the panel
and projection, rather than changing fog or range lighting. The earlier
provisional lighting label has been corrected; restoring that shake remains a
separate increment.

All **115 regressions**, the 11,232 contact gates and 8,192 damage/repair
cases pass in development, Release and ASan/UBSan. The private silent ejection
check also completes ordinary double-Escape, authored seat release, parachute
and recovery/FINISH after the correction; it is not a post-ejection projectile
hit reproduction.


### Flak cockpit shake and frame order

The remaining flak callback is now connected to the shared cockpit-position
controller. Close daytime bursts save the panel offset, restore the base before
restarting an interrupted shake, and follow the original signed-word sequence
from `0x6044`. Its sentinel ends the callback. Changing away from the forward
cockpit cancels the shake and restores the saved offset. Raising/lowering the
panel can replace that callback and updates the same saved position, as in the
original. The saved-Y `-1` sentinel and conditional X restoration are retained.

The frame loop advances the cockpit callback after player physics/cannon and
before timed messages and the flak controller, matching native `0x720b8`.
A newly requested flak shake
therefore starts moving on the next frame. Panel, projection and HUD consumers
already share this position. This restores positional feedback; it does not
change fog, range lighting or aircraft forces. The separate aerodynamic ground
bump callback and sun-glare producer remain unbound.

`check_cockpit_shake.py` executes **8,192 complete original positioning
callbacks** across repeated shakes, reset/restoration, raise/lower
interruptions, camera changes, external offset changes and word-wrap/sentinel
boundaries. Every observed offset, panel-step and callback state agrees. No
numerical or state dependency is intercepted. The existing **2,048 original
raise/lower steps** and all six instrument-start cases still agree. These
checks load tables from the user's executable; full panel-pixel comparisons
remain in their existing independent harness.

All **115 regressions**, 8,192 shake cases and the existing positioning/setup
comparisons pass in development, Release and ASan/UBSan. The private daytime
flak test observes the live shake request, burst model and palette flash in
development and sanitizers, then exits cleanly. Its region/height/random inputs
remain explicitly controlled rather than a naturally reached flak encounter.


### Sunlight glare and the complete flash selector

The live day-mode palette now uses the complete native `0x73838` producer.
Pending flak has priority and requests level 40. Otherwise a fixed world
direction is projected relative to the integer camera anchor, retaining word
source coordinates and the native reciprocal point projector. Clouds conceal
the sun below their layer; target view excludes ordinary glare. Inclusive
viewport bounds and the original signed-word max-plus-half-min distance select
all 50 white-tint levels. Night environments leave this producer disabled.
The inverse night/day-glare binding is verified for all 33 native environment
parses, including twilight.

Current camera projection is prepared before palette selection and shared with
impact classification. The panel viewport supplies its current bounds and
projection centre. This restores the glare effect through the existing
altitude/protected-colour/white-tint painter; the separately authored sun model,
full sky renderer and whole-frame equivalence remain outstanding.

`check_glare.py` compares **8,192 complete original producer cases**, including
all **50 output levels**, real matrix/translation/viewport setup and original
point projection, cloud-height equality, target-view suppression, viewport
edges, pending-flak precedence and signed-distance/word-wrap quirks. Nothing
is intercepted. Synthetic camera and gate state are supplied explicitly. A
compact asset-free fixture runs in CTest.

All **116 regressions** and the full glare cases pass in development, Release
and ASan/UBSan. A private controlled sun-facing projection reaches the real
selector, strongest glare level, palette painter and world draw in development
and sanitizers. The window observer now waits for flight-input readiness and
holds quit until consumed, avoiding a short key tap being missed during
startup. This is a supplied-input handoff check, not full sunlight/scene pixel
equivalence.


### Secondary scenery blasts and synchronous SSD outputs

SSD opcode 79 now delivers its authored boom/fireball requests to the live
100-slot effect pool. The earlier provisional `light` label was misleading:
native `0xb626c` routes through `0x7b0cc` to the same allocator used for impact
effects. It is separate from model-authored amber night dots and from dynamic
lighting. The retained corpus has **782 requests: 699 booms and 83 fireballs**
in cities, factories, defence sites, bridges, docks, ships and other scenery.

The scene owns local placement; flight owns the pool and its independent
models. A borrowed synchronous request callback preserves interpreter ordering
through events, contacts, animation and drawing. Terrain placement uses its
exact quarter turn and signed-word origin; moving SSD request placement remains
separate from the full visual Euler transform. Original actor initialization
binds the relative-reference pointers to the player's integer coordinates,
which cancel through the emission callback before the pool's word stores.
Effect submissions are gathered after world SSD execution and before the
global part-depth sort, including the existing scene-camera display path.

**11,320 original placed requests agree**: all 782 authored requests at all four
quarter turns, plus 8,192 boundary cases across all ten effect kinds and
independent player references. Native reference binding, quarter rotation,
origin addition and player-relative helpers execute. Allocation is observed at
its separately compared boundary and final stored word positions are checked.
An original authored SSD additionally verifies that request callbacks observe
custom states 11 and 22 before the final change to 33; delivery cannot be
deferred until interpreter return. Compact fixtures and the request-order case
run without game assets.

A controlled real factory scene damages its authored collision volume, advances
32 animation/draw passes, resets actual effect models and submits the resulting
blast in the same frame. Its emitted position and animation frame agree with
complete original factory bytecode and the placed-emission helpers. Model
resource/drawing and allocation output remain declared boundaries; this is not
a complete mission or pixel comparison. The full **703-definition / 19,589
persistent-state** SSD corpus still agrees; its initialization calls also check
that no opcode-79 effects are discarded by construction in that capture.

All **118 regressions**, placement/order cases and the controlled factory scene
pass in development, Release and ASan/UBSan. The private controlled cannon
impact/retirement window test still reaches its real effect request, pool,
placement and world draw in development and sanitizers. Smoke trails, legacy
offset consumers, ground shadows and full renderer/session equivalence remain
separate work.


### Player-referenced SSD range and anchor queries

The scene now borrows the player's live integer position for SSD range queries,
matching the original actor-initialization bindings at `0x7589a`. This
reference is independent of the chase, fly-past, target and scene-MFD cameras.
Inspection clears that borrowing and uses its camera reference instead.

Previously ordinary SSD drawing supplied distance zero, and measured anchors
from the camera with the full visual transform. Drawing and animation now
supply the native word distance from the instance origin, and anchor queries
use the original quarter-turn placement followed by word subtraction and
`0xcbe94`. Moving SSD anchor placement remains separate from its Euler-rendered
parts. Exact placement is shared with effect emission in the instance module;
signed distance wrapping and native thresholds are retained.

The placed-request comparison now also executes the complete original
`0xb6be8` anchor query for **11,320 cases**, covering all authored effect
positions/quarter turns and synthetic extreme/reference cases. The controlled
factory scene now explicitly supplies a player reference different from its
camera and runs the matching original range setup through its 32 passes.
All range, placement, request-order and factory comparisons pass in development,
Release and ASan/UBSan, along with all **118 regressions**.

All **128 retail missions** additionally start, render and run 64 integrated
steps in development. Only the same two pre-existing unavailable-vertex notes
remain. The private controlled cannon-impact window check also still passes.
These are scene robustness and declared query-context checks; complete original
visibility/update eligibility, collision caller context, world pixels and
whole-session equivalence remain separate gates.


### Missile smoke and native bounding-volume visibility

The original post-motion missile path now feeds a separate **200-slot smoke
pool**, with independent `smokecld` SSD state for every slot. Eligible stores
emit within 4,000 player-relative units; attached and falling callbacks are
excluded, while acceleration can already produce smoke. World, weapon and
user detail gates retain their separate owners. The trail segment is tested
against the actual camera before sampling at the native ten-unit spacing.

The shared camera bounding-volume test translates `0xcbef0` / `0xb9c00`,
including the viewport apertures patched by `0xb94da`, word-coordinate
transforms, behind-camera extent test, far limit and signed ABS quirks. Its
**16,384 complete original cases** pass; the existing integer/Q16 point
projector's 16,384 cases still pass after sharing its transform.

**8,192 original segment cases** agree in their midpoint/radius handoff and
all **1,354,082 emitted Q8 positions**. This includes signed-word length
wrapping and its unusual signed-IDIV/loop behaviour on extreme segments.
**4,096 complete original pool operations** agree in all 200 resource bindings,
oldest-slot reuse, model reset order, records, Q16 drawing poses, fractional
position/seam handling and draw-before-cleanup. Only a consecutive expired or
more-than-8,000-unit prefix is retired, as in the original; there is no invented
particle lifetime. Original resource delivery/model destruction and the
independently checked visibility consumer are declared capture boundaries.
The stateful fixture retains its entire operation sequence.

**8,192 original post-motion dispatch cases** additionally agree across the
world/weapon flags, all thirteen motion callbacks, strict 4,000-unit boundary
and exact Q8-to-world-word callback arguments. The supplied player-distance
query is a declared boundary; warning and ground-impact phases have their own
comparisons. A private silent window test injects a nearby missile and reaches
the real dispatch/visibility gates, pool, fractional placement and scene draw,
without overriding visibility. This establishes the live handoff, rather than
claiming a naturally played encounter or whole-frame pixel equivalence.

Reproduce with `tools/check_trails.py`, `tools/check_visibility.py` and
`tools/check_window_effects.py --trails --data-dir /path/to/TFX`. Compact
captures run as four additional asset-free CTests. Full renderer eligibility,
weather, ground shadows and complete mission/session equivalence remain open.

All **122 regressions**, complete trail/visibility comparisons and the private
smoke handoff pass in development, Release and ASan/UBSan (the private window
uses development and sanitizer builds). The existing **5,796 original
projectile-population frames** also still agree.


### Sun, storm lightning and upper cloud scenery

The live frame now submits the original `sun1.3` textured model, suppresses it
in twilight and below the cloud layer, and produces `lightnin.3` strikes in
storms. Lightning retains its independent countdown and two-word generator
across missions. A zero countdown wraps instead of firing immediately. Strike
positions are word-wrapped for drawing, while the full stored position remains
available to the palette consumer. A pending strike produces its native level-12
white tint once its ground-relative flash point is strictly inside the view.
The existing separately checked camera-volume test guards model submission.

The scene can queue a shape directly, without inventing an SSD wrapper. These
sky shapes join the same depth ordering as scenery and moving actors, retain
their authored primitive order and resolve their original global texture page.
The separate cloud grid is loaded from the environment's named layer resource;
above and exactly at the layer height, the native frame chooses that grid at
its signed-word height plus offset, rather than drawing the regional ground.
Collision and mission target queries retain the regional grid. The frame's
camera anchor selects the layer independently of its fractional camera pose.

`tools/check_sky.py` executes **8,192 complete original sun/lightning requests**,
**4,096 complete pending-flash/projector cases** and **4,096 actual main-frame
grid-selection branches**. They agree in state, model/position requests, 461
strikes, 247 visible flashes, independent player/camera gates, timer/RNG wrapping,
strict viewport bounds and cloud-height equality. Model submission, external
resource handles and grid traversal are declared boundaries; original point
projection and state arithmetic execute. The 33 native environment captures
also verify twilight suppression. Compact captures are asset-free CTests.

Controlled day/storm/night scenes render below, at and above the cloud layer.
Separate sun/lightning scenes assert that each submitted model changes visible
pixels, including its native texture/patterns; captures remain ignored scratch.
This checks resource delivery and composition, not native whole-frame pixels.
All **123 regressions** and the complete sky comparisons pass in development,
Release and ASan/UBSan. All **128 retail mission scenes** also run 64 integrated
steps, with only the same two pre-existing unavailable-vertex notes. The original
horizon bands, precipitation, full renderer eligibility/lighting and complete
sessions remain open.


### Original banded horizon and moving ground phase

Live flight and scene displays now paint the native `0xcaf84` backdrop instead
of the inspector's flat horizon. It retains the original sky/ground palette
bands, altitude-dependent projected boundaries, steep-pitch fill, banked edge
caps and inverted views. No RGB interpolation or new atmosphere is introduced.
The missile nose horizon uses its native projection shift. Free scene
inspection retains its independent simple backdrop.

The horizon's solid polygon path now has the original signed-word winding,
bounding-span overflow clamp, bottom/top/right/left clipping order, farther-end
IDIV intersection rounding, reciprocal Q16 edges, half-step bias and inclusive
spans. These routines are separate from the still-in-progress general model
painter. **8,192 complete original surface programs** agree in every emitted
vertex/index, with **1,024 complete original VGA frames** matching pixel for
pixel. A further **192 wider-projection VGA frames** agree. Raster callbacks
execute unchanged; only the camera/viewport/phase and video aperture are
supplied. Compact surface and complete-pixel hashes run without game assets.

**8,192 complete original ground-phase updates** also agree. The counter is a
word, with a separate adjacent period-mode word; updating the counter must not
overwrite that mode. Signed movement shifts, word comparisons and the original
single period correction remain. Arcade and Simple supply their own movement
notifications; the camera supplies its separately rotated horizontal-distance
notification except in chase view. The horizon consumes the phase before that
camera update, matching the original frame order.

The camera now exposes whether a horizon was actually submitted, its angles,
and the camera height at that submission. Tactical, unbanked chase and ejection
views draw before installing their new translation; a missing impact target can
skip the horizon for one transition frame. **12,288 complete original camera
updates** agree in these handoffs, ground-motion inputs and retained reference,
alongside the existing pose/matrix/queue state. **4,608 complete player updates**
across 48 initialized scenes and manual/autopilot/model handovers additionally
agree in the actual phase state, covering all four flight models and disabled
paths. Numerical flight helpers and their original phase consumer execute.

All **127 regressions**, complete surface/phase cases and camera/player handoffs
pass in development, Release and ASan/UBSan. Controlled normal-projection VGA
frames pass in each configuration; the larger 1,024-frame and wide captures use
development, with compact pixel fixtures checked in all three. All **128 retail
missions** additionally render and run 64 integrated steps, retaining only the
same two unavailable-vertex notes. Private silent camera switching passes in
development and sanitizers.

Reproduce with `tools/check_horizon.py`, `tools/check_horizon_pixels.py`,
`tools/check_horizon_phase.py`, `tools/check_view_cameras.py --horizon` and
`tools/check_player_flight.py --horizon --model-changes`. Complete world/MFD
pixels, general model lighting/remaps, driver projection-mode configuration,
precipitation and whole-session equivalence remain separate gates.


### Original storm precipitation and startup random order

The native 300-particle field (`0x78af8`) and frame painter (`0x78d8c`) now
render in live flight. These are single palette-index pixels, with the original
camera-biased wrapping volume, Q15 transforms, depth bias, signed projection
and viewport clipping. Wind advects the retained offsets once per drawn frame;
the vertical offset advances by 30. A zero particle count still advances the
field. Pausing and excluded frames retain it. Mission initialization replaces
the particles while preserving drift, as in the original.

The main-frame gate (`0x722c2`) uses the precipitation preference, storm state,
strict player/cloud-height comparison and view selection. Virtual cockpit and
target tracking omit rain. The original signed-speed calculation supplies the
particle count. Rain paints after the world queue and before HUD/cockpit
artwork, including the current cockpit viewport.

Startup now supplies weather and rain from the retained session random stream,
then uses the last loaded terrain grid's reset stream for actors and gameplay.
This preserves the original ordering: player/weather initialization, 1,800
rain-field random draws, base grid, optional upper grid, actor setup. Each
mission returns its gameplay stream for the next startup. Complete menu-to-flight
and warm-session global equivalence remain separate verification boundaries.

`check_precipitation.py` compares **2,048 complete original VGA frames** pixel
for pixel, along with the full initialized particle field, random words and
x87 drift. **8,192 original main-frame gate cases** cover height equality,
view exclusions, signed speed/wrap and unordered floating inputs. The extended
`check_weather_bindings.py --rain` compares **1,056 complete weather-to-rain
initializations across all 33 authored environments**, including all ordered
shared random draws. Numerical helpers and VGA writes execute unchanged;
camera, viewport, wind and random seed are supplied inputs. Compact captures
run without game assets. Real mission composition and private storm camera /
pause checks additionally exercise the live delivery path.

All **129 asset-free regressions** pass in development, Release and ASan/UBSan.
The complete gate and 1,056 startup comparisons repeat in each configuration;
Release and sanitizer builds additionally match 256 complete original rain
frames each. All **128 retail missions** run 64 integrated steps, retaining only
the two existing unavailable-vertex notes. Real rain/cockpit composition and
private camera/pause checks pass in development and sanitizers. Reproduce the
live composition check with `tools/check_precipitation_scene.py`; its temporary
silent configurations never alter an original install.


### Original missile-nose projection aperture

The camera now carries its original normal or missile-nose aperture to model
projection, sphere visibility and the horizon. Native `0xca780` resets the
mode on each update; view 7 applies `0xca6fc(-1,-1)`, changing the model focal
length from 256 to 128 pixels and the corresponding sphere-culling scale.
The state is explicit rather than inferred again from a potentially queued
next view. The separately encoded HUD/world-point projector and rain projector
retain their own unchanged arithmetic.

**8,192 complete original model projections** agree in both modes, including
**57,344 camera-unit scale cases**. The original mode setters patch and execute
the real projection loop; Q16 camera vertices and actor/camera translation are
supplied inputs. A further **16,384 complete widened sphere tests** agree;
the existing 16,384 normal tests still pass. The complete camera comparison
now executes the original driver setters too, checking **12,288 aperture
handoffs** alongside pose, matrix and horizon submission state.

All **131 regressions** and the widened model/sphere/controller comparisons
pass in development, Release and ASan/UBSan. Normal model vertex, orbit and
scaled projection comparisons continue to agree. All **128 mission scenes**
run 64 steps. Private control checks in development and sanitizers exercise
actual store firing, F8 follow/nose views and F1 restoration, MFDs, ground
weapon/designator controls, pause and a combat-to-FINISH result. Complete
world texture, lighting and polygon coverage remain separate renderer work.

Reproduce with `tools/check_aperture.py`, `tools/check_visibility.py --wide`,
`tools/check_view_cameras.py --projection --horizon` and
`tools/check_window_controls.py`.


### Native flat model-face pixel coverage

Flat, untextured, unrounded model faces wholly beyond the native near plane
now use Q16 projection followed by the verified integer polygon raster. This
replaces the research viewer's half-open spans and floating side-plane
intersections for that path. The original signed clipping order, winding,
reciprocal edges, inclusive spans and endpoint rounding are retained. Inspection
cameras keep their independent raster. Near-plane crossings, shaded, patterned,
rounded and textured faces retain their separate paths pending verification.

The camera owns explicit native viewport bounds. Flight supplies the current
cockpit rectangle and scene displays supply their MFD rectangle; the flat
raster, horizon and bounding-volume setup share those bounds. This fixes
coverage at the actual clip boundary rather than relying on later cropping.

`tools/check_world_polygons.py` runs the original `0xb9434` projection and
`0xbd3a8` flat raster through VGA writes. **1,024 complete controlled frames**
agree pixel for pixel across normal/wide views, camera-unit scales, windings,
degenerate faces and partial cockpit/MFD rectangles. Input camera vertices,
translation, colour/remap and viewport are explicit boundaries; near crossings
and full-scene transform/order/lighting are separate gates. The 128-triangle
baseline changed from **114 differing frames to zero**. Compact pixel hashes
run without game assets, and all 1,287 existing scale/near-clip property cases
continue to agree.

All **132 regressions** pass in development, Release and ASan/UBSan. Release
and sanitizer builds also match 256 complete original flat-face frames each.
All **128 retail missions** run 64 steps. Private controls in development and
sanitizers cover firing, missile cameras, MFD scene pages, designator controls,
pause and combat-to-result flow. Live storm/cockpit composition still passes.


### Native near-plane clipping for flat faces

Flat model polygons now also follow the original near-crossing route
`0xd7998`/`0xd7be5`. Vertices with signed-word depth at or below 16 remain
unprojected until their edges are clipped. `0xd7d22` bisects word depths,
records a low-bit-first decision path and replays wrapped Q16 XY midpoints.
Its equality/sentinel cases are retained; replacing this with a real-valued
line intersection changes the output. The clipped projection (`0xd7c90`)
uses arithmetic shifts, normal/wide limits and direct ±32,000 saturation,
which deliberately omits the origin on saturated axes.

**16,384 complete original edge intersections and projected points** agree,
including extreme signed-word and dword wraps, fractional source depth,
normal/wide modes, saturation boundaries and screen-word origin wrapping.
**1,024 complete original near-crossing VGA frames** agree pixel for pixel,
covering both crossing directions, exact near depth, all-behind faces,
clipped winding and partial viewports. The 1,024 ordinary flat-face frames
still agree. Camera coordinates, remap/colour and viewport remain supplied
inputs; model-facing bytecode, full transforms, general shading/textures and
whole-scene order remain separate gates.

All **134 regressions** pass in development, Release and ASan/UBSan. The full
edge comparison repeats in all three; Release and sanitizers also compare
256 complete near-crossing frames each. All 1,287 scale/near-clip properties
still pass. All **128 retail scenes** run 64 steps, and private development /
sanitizer controls cover firing, missile follow/nose views, MFDs, ground
controls, pause and combat-to-FINISH. Reproduce with
`tools/check_near_clip.py` and `tools/check_world_polygons.py --near`.


### Native model-facing branch and interpreter handoff

The shape interpreter now distinguishes opcode 21's branch decision from the
raster's polygon winding. Fully projected branches subtract sign-extended
screen words in dwords, with wrapped multiplication/subtraction. Polygon
winding instead narrows its differences to words. If any of the branch's three
vertices is unprojected, native `0xbb890` builds a wrapped cross product from
word camera-coordinate differences, shifts every normal axis together until
all fit a word, then tests a wrapped dot product against the negated second
vertex. Degenerate near faces consequently differ from ordinary screen tests.

Live native cameras supply those decisions through an explicit shape-context
callback; inspection retains its separate projection. The branch's temporary
vertices now retain the same camera-unit scale as the controlled surfaces.
Flat raster and facing share Q16 conversion, near clipping and integer
projection, including native origin-word wrapping. IDIV overflow/out-of-range
inputs retain the host's separate fallback rather than invoking undefined C++
conversions.

**16,384 complete original model-facing branches** agree across normal/wide
modes, camera scales, screen-dword overflow, word normals, shared shifts,
degeneracies and origin wrapping. **1,024 complete synthetic model programs**
additionally execute the original resource converter, absolute vertices,
translation scaling, projection, facing branch, colour reader, flat/near
raster and return dispatch. Their branch-controlled draw counts and every VGA
pixel agree with C++. The capture supplies model bytes, identity camera/actor
matrix, translation, viewport and an explicit identity remap; no model
instruction, numerical helper or drawing helper is replaced. This verifies the
interpreter handoff, while arbitrary actor transforms, distance remap selection
and whole-scene equivalence remain separate gates.

Both 1,024-frame flat and near-crossing comparisons still agree. All **136
regressions** pass in development, Release and ASan/UBSan; full facing and
256 complete model-program comparisons repeat in Release and sanitizers.
All **1,012 extracted models / 2,042 detail-state cases** continue to match the
independent inspection VM. All **128 retail scenes** run 64 steps. Private
development/sanitizer controls still cover weapons, missile cameras, MFDs,
designator, pause and combat-to-FINISH. Reproduce with
`tools/check_model_facing.py` and `tools/check_model_programs.py`.


### Original distance colour tables and submission binding

Native flight now loads the root `coltab.dat` (4,096 bytes: sixteen rows of
256 palette indices), preserving the original identity fallback when absent.
This is distinct from the smaller `dat/coltab.dat` selector resource. Retail
frame setup uses fade shift nine; signed camera-depth high words select a
clamped row from zero to fifteen. Unanchored SSD parts share their
assembly-origin row; anchored parts use the cache selected by their own
integer or fixed query. Direct sky models use integer-coordinate submission. UI artwork and inspection cameras retain
their separate colour paths.

Flat surfaces, lines and circles remap their ink once. Gouraud and texture
painters remap the interpolated or fetched index. Texture transparency is
tested **after** the lookup, preserving both zero-to-visible and
nonzero-to-transparent cases. These are discrete palette lookups, without
RGB interpolation. Native camera and point queries now share one adapter for
Q16 camera positions, Q15 matrix rows, aperture and viewport.

`tools/check_colour_remaps.py` compares **16,384 original depth selections**
and every byte produced by original `0xd7954` table initialization with the
live C++ asset owner. `tools/check_placed_colour_rows.py` compares **8,192
complete word/fixed position-to-submission paths**: original matrix/camera
setters, sphere-query cache, `0xcc0bf`/`0xcc164` queue entries and sort insertion
execute; supplied model-handle resolution is the sole intercepted boundary.

`tools/check_remapped_renderer.py` compares **256 complete flat/circle frames**
and **1,129,184 Gouraud/texture interior pixels** across every actual remap row,
including the transparency-order cases. General shaded/textured edges, full
world transforms and whole-scene ordering remain separate verification gates.
The shared camera adapter also retains all 16,384 original point-projection
comparisons.

All **138 regressions** pass in development, Release and ASan/UBSan. The
complete row/asset/submission and remapped-pixel comparisons repeat in all
three builds. All **128 retail scenes** advance 64 steps, private development
window controls still pass, and storm/cockpit composition remains correct.


### Native Gouraud edges, near clipping and model-program delivery

Live Gouraud model faces now share the verified integer screen clipper and
reciprocal edge walker with flat faces. Clipping retains the original
farther-endpoint division and signed-word differences for both coordinates
and 8.8 colour attributes. Each edge retains its fractional colour accumulator;
the horizontal painter uses the original rotated ADD/ADC or SUB/SBB state,
including the initial fraction-only phase. The latter matters when colour
differences wrap across the full attribute word. Lookup follows interpolation,
without blending RGB values.

Near crossings follow original `0xd8074` / `0xd809b` / `0xd824e`. The word-depth
search shares its decision path with the flat clipper, but colour midpoints
use unsigned dword sums while XY uses wrapped signed sums. Exact-near
vertices retain their original attribute. The resulting vertices use the
same saturated normal/wide projection and inclusive raster. Inspection and
the still-separate textured/patterned/rounded paths retain their own painter.

`tools/check_gouraud_polygons.py` compares **1,024 complete original VGA
frames**, covering fractional shades, clipped convex faces, both lookup
directions, partial viewports, single-row/column extents and degeneracies.
`tools/check_world_polygons.py --gouraud` adds **1,024 complete projected
frames**; adding `--near` compares another **1,024 near-crossing frames**,
including depth/coordinate wraps and camera-unit scales.

`tools/check_model_programs.py --gouraud` executes **1,024 complete synthetic
programs** through the original endian converter, absolute vertices, scaling,
translation, projection, facing branch, packed colour instructions, Gouraud
face instructions, near clipping and VGA writes. Every pixel and the
branch-dependent attribute/draw count agrees. Original code and drawing
helpers execute without intercepted dependencies; model bytes, identity
matrices, translation, viewport and remap remain declared inputs. Full actor
transforms, lighting eligibility and whole-scene order are separate gates.

A further **1,024 full-word colour cases** retain signed attribute-difference
wrap and rotated carry/borrow behaviour. Tracing the original span arguments
isolates the one-step differences caused by replacing that state with an
ordinary fixed-point accumulator; the native carry path now agrees. Compact
fixtures retain the cross-ramp cases as well as ordinary shading.

All **142 regressions** pass in development, Release and ASan/UBSan. Release
and sanitizers each repeat 256 complete original screen/near/model-program
comparisons. All **128 retail scenes** advance 64 steps. Private development
and sanitized windows retain weapon, missile-camera, MFD, designator, pause
and combat-to-FINISH coverage; storm/cockpit composition also still passes.


### Native model range, detail representations and SSD placement

Live flight now computes original cached model metrics before interpreting
a shape: the wrapped dword estimate `largest absolute axis + (other axes)/4`,
its signed high-word range, signed depth-plus-bias priority and distance
remap row. Shape headers retain their signed minimum range and seven detail
slots separately from the priority word. Exact signed threshold comparisons
select the authored polygon entry, two-pixel block or single pixel. The
reserved detail kind retains the original converter's unsupported status.
Inspection can still select polygon levels independently.

SSD draw data retains three distinct origins: admitted fixed assembly cache,
integer anchor query and fixed anchor query. Anchors use the original word
quarter-turn placement, independently of the part's full visual Euler
rotation. Normal anchored draws and damaged overlays use the fixed-query
variant; the rotated/special anchor variants use the integer query. Each
part consequently receives its own appropriate detail range and remap row.
The actual SSD bounding sphere now admits native assemblies and anchors
before their drawing program/part submission. This does not yet establish
exact animation-update eligibility or the complete queue lifecycle/order.

Distant representations use the original signed division and clipped anchor.
Their two-pixel callback can write past a viewport edge and wrap the physical
right edge to the next VGA row. Unlike model polygons, both distant-point
handlers retain their normal shift even in missile-nose view. These quirks
are retained independently of the host's polygon projection and far plane.
Original IDIV-overflow inputs have no defined native pixel result and are
excluded from the pixel comparison; the host declines those inputs safely.

`tools/check_model_metrics.py` compares **8,192 complete synthetic native
submissions** through conversion, queue insertion, range/header selection,
priority and remap. Its corpus mode compares **13,663 header selections
across all 1,012 models**, including threshold neighbours and wrapped axes.
Corpus capture executes the original header-conversion stage, declaring
polygon instruction conversion and the separate missing-body diagnostic as
boundaries; selected polygon/point drawing is observed rather than executed
in that selector-only comparison.

`tools/check_model_placements.py` executes **8,192 complete part constructions
across all six normal constructors**, retaining integer/fixed queries,
all quarter turns, signed words, camera fractions, normal/wide bounds and
native sphere admission. Only SSD handle resolution, geometry submission
and the subsequent notification callback are supplied boundaries. All
**703 SSD definitions / 19,589 persistent states** additionally retain the
constructor's placement type, including damaged overlays.

`tools/check_model_points.py` compares **933 complete original VGA frames**
across the defined distant-point domain. `tools/check_model_lod.py` adds
**1,024 complete queued multi-level model frames**: original resource
conversion, queue construction, metric/header choice, polygon constructor,
model bytecode, projection/near clipping and both distant-point painters
execute, with no intercepted model or raster helper. The sole intercepted
callee resolves the supplied model handle. Model bytes, identity view
matrix, raw camera cache, viewport and identity remaps are declared inputs.

All **146 regressions** pass in development, Release and ASan/UBSan. Release
and sanitizers repeat all 8,192 six-constructor placement cases and 256
complete queued-model VGA frames, alongside the range/header and distant
point comparisons. All **128 retail scenes** advance 64 steps without preview
warnings. All 1,012 models / 2,042 detail-state interpretations remain equal
to the independent research VM. Private development/sanitized controls,
real factory effects and storm/cockpit composition still pass.


### Native bounded model queue and submission ownership

Flight now uses the original **348-slot** queue instead of sorting an
unbounded vector. Signed priorities, the fifteen-entry quick-sort threshold,
small partition runs, final insertion pass and equal-key movement follow
`0xc4750` / `0xc4824`. The first fill and subsequent replacement paths remain
distinct: original `0xc4930` changes its own branch after filling, and later
submissions reuse the previous last entry's payload slot. Rendering traverses
the resulting slot list backwards, as `0xc49d9` does.

The queue owns ordering/slot state, while the scene owns each submitted model
reference, name, transform and **eight copied parameter bytes**. This retains
the original constructor's parameter snapshot rather than reading a mutable
instance during the later draw. Inspection retains its independent full list.

`tools/check_model_queue.py` runs original reset, every insertion/replacement,
partition/insertion sorts and complete flush. **512 complete sequences /
180,751 slot allocations / 149,241 draws** agree, including empty and tiny
queues, 14/15/16-entry boundaries, repeated/signed priorities, exact capacity
and prolonged replacement. Payload IDs are supplied; the geometry consumer
and post-draw notification are observation/no-op boundaries. Compact native
slot-allocation and draw-order hashes run without game data. The original
minimum-word insertion sentinel is excluded from the native comparison; C++
uses an explicit lower bound for that input. Immediate submissions, shadows
and complete renderer-phase scheduling remain separate gates.

All **147 regressions** pass in development, Release and ASan/UBSan. Release
and sanitizers repeat 128 complete native queue sequences each. All **128
retail scenes** advance and render without preview warnings. Sanitized
private-window controls retain weapons, cameras, MFDs, pause and FINISH flow.


### Complete composed-flight frame coverage

The developer `--flight-frames N` runner now executes the same ordered stages
as the live loop: an initial composed scene, then world update, complete draw
and frame-tail/result processing for each supplied four-tick step. Rendering
therefore participates in camera state, HUD retention, scenery/effect requests
and following-frame decisions. It can capture the resulting scene without a
window and stops on a real mission result. The existing `--flight-steps`
remains a fast numerical warm-up without intermediate drawing. Both options
remain absent from Release's command-line interface.

These runs supply neutral controls and fixed tick/physics intervals, bypassing
the host clock/input sampling. They are C++ integration/reproducibility checks,
not full original-session comparisons. Reproduce corpus coverage with
`tools/check_mission_scenes.py --research ../tfx-reverse --flight-frames 64`
and select the other models with `--flight-model simple`, `military-spec` or
`realistic`. All **128 retail missions × 64 complete frames × all four flight
models** pass in development, without preview warnings. ASan/UBSan repeat
**all 128 missions × 64 composed frames** without warnings or sanitizer errors.

Longer neutral runs use `--mission`, `--flight-model`, `--flight-frames 4096`
and `--screenshot /dev/null`. FWS mission 501 reaches an ordinary loss/FINISH
after **2,780 frames**, and Arcade mission 605 after **445 frames**; no damage
or outcome values are injected. Tour missions 801 (Military-spec) and 830
(Realistic) each complete **4,096 composed frames** cleanly. These exercise
actual lifecycle outcomes, not successful player-controlled mission completion.

Repeated 64-frame Arcade mission-501 captures using the same disposable
silent installation agree byte for byte in development and sanitizers; their
PPM SHA-256 is `20cdd6250b457c8cdfa343325f6d2b7530a6d42513cbcfaa4011246ea5ee7ea0`.
All **147 asset-free regressions** remain green. This runner improves the
coverage of render-dependent state without implying original whole-frame or
whole-campaign equivalence.


### Complete patterned model-face raster coverage

Unrounded, untextured patterned model faces now use the same native Q16 near
projection, signed-word viewport clippers and reciprocal integer edge table
as flat faces. Painters consume that table from bottom to top, retaining
empty endpoint spans and procedural byte-generator consumption. The shared
pattern painter also retains the existing circle and inspection paths.

All nine modes (0, 1, 2, 3, 4, 7, 10, 11 and 12) have complete original VGA
comparisons, including checker/sparse stipples, random masks and palette-index
XOR/AND/OR. The sparse painters have distinct narrow-byte skip phases and
endpoint counts. The line-stipple callback's original `0xe4ccf` one-bit left
shift after a single-byte span is preserved; this can terminate a tapered
face early. This is an original raster quirk, not a new culling rule.

`tools/check_world_polygons.py --pattern` compares **1,024 complete frames**;
adding `--near` compares **1,024 more** through original near clipping. Every
pixel and retained procedural seed agrees, across normal/missile-nose
projection, signed coordinates, reverse/degenerate winding, scaled model
units and cockpit/MFD viewports. Compact frame hashes and final seeds run
without game assets. The VGA verification aperture now models read latches
and write-mode-0 logical operations instead of treating all writes as direct
colour stores. Flat and Gouraud captures remain unchanged. Rounded/textured
edges, complete actor transforms, shadows and whole-scene scheduling remain
separate verification work.

All **149 regressions** pass in development, Release and ASan/UBSan. Release
and sanitizers each repeat 256 complete patterned near-plane comparisons.
The existing 27 raster interiors, nine complete circles, three procedural
seed transitions, 44 rounded corners and 1,280 Gouraud samples still agree.
All 128 retail missions advance and compose 64 complete frames without
preview warnings after this renderer change.


### Native outer model transforms and SSD orientation

Live native scenes now retain each actor's Euler words rather than only a
floating matrix. The SSD constructors add a rotated part's Euler metadata to
the actor words, wrapping each sum; they do not multiply independent actor
and part matrices. Normal constructors also add the terrain quarter-turn.
Special opcodes 46/54 intentionally retain world-axis orientation despite
using the normal quarter-turned anchor placement. Unrotated bindings ignore
the metadata triple. These rules now have explicit part-orientation records.

A submitted model owns its cached camera translation and quantised view/body
basis. Original `0xb972a` wraps each dot product, shifts Q15 and stores a word;
all-zero Euler words bypass the extra component matrix. Translation uses the
actual integer/fixed camera cache already captured at submission, preserving
its wrapping and per-term high-product rounding. Signed size flags divide
or multiply that cache before detail selection and drawing, while the queue
priority and colour-remap row retain their original pre-scale cache values.

`tools/check_model_orientations.py` compares **8,192 native constructions
across all six origin/anchor paths**, observing the submitted angle words and
size flag after actual quarter-turn callbacks and SSD constructors. Geometry
submission and the supplied instance handle are declared boundaries.
`tools/check_model_transforms.py` executes **1,024 complete queued, converted
model frames**, agreeing in all nine basis words and every VGA pixel. Source
absolute vertices, model scale, actor Euler angles, view matrices/caches,
aperture and identity colour tables are inputs. Only supplied model-handle
resolution is intercepted; matrix generation, queue construction/flush,
model bytecode, near clipping and VGA drawing all execute. Random full-word
matrices exercise wrapped arithmetic as well as ordinary view matrices.
Compact basis/frame fixtures run without original game files.

This closes the outer-pose/cache path for those programs. It does not claim
complete fidelity of model-local articulated matrices, carried-model matrix
stacks, rounded/textured edges, shadows or whole-scene scheduling. All 128
retail missions also complete 64 composed frames without preview warnings
with the new transform path.

All **151 regressions** pass in development, Release and ASan/UBSan. Release
and sanitizers each repeat 256 complete native transformed-model frames.
All 703 SSD definitions / 19,589 persistent states still agree. Private-window
controls retain three MFD displays, weapons, follow/nose cameras, ground
weapon designation, pause and a real combat FINISH/return flow.


### Original ground-shadow submission and drawing

SSD ground-shadow requests are now rendered instead of discarded. The
original `0xb6a44` cutoff admits object Y in **(-1024, 0]**. The shadow query
retains the actor's fixed X/Z fractions and uses the current global
player-ground-height word (`0xabf2`) for its plane; it does not independently
sample terrain beneath each aircraft. This original shared-plane behaviour
is retained in both the main world view and secondary MFD cameras.

Shadows enter the same bounded queue with their own camera cache, priority,
colour-row state and copied model parameters. Original `0xbacb1` constructs
the component matrix, applies its X/Z flattening matrix through `0xcc97c`,
then composes the view matrix with the usual word stores. The additional
32767 diagonal quantisation is retained even at zero Euler angles. Shadows
only use polygon detail levels; their distant point representations do not
draw. Forced black drawing discards authored Gouraud colours and texture
bindings while retaining the model's geometry and facing decisions.

`tools/check_model_shadows.py` executes **8,192 complete original shadow
placements**, checking the height boundaries, signed shared ground height,
fixed camera products, sphere predicate and normal/wide viewports. Supplied
instance resolution and subsequent geometry submission are boundaries.
`tools/check_model_transforms.py --shadow` compares **1,023 complete native
shadow VGA frames**, including all nine matrix words. `--shadow --gouraud`
repeats them with authored Gouraud faces and the original forced-flat path.
One synthetic input overflows original word-IDIV and has no completed native
frame; it is explicitly excluded. Of the defined inputs, 417 paint pixels.
The original converter, queue construction/flush, matrix generation,
flattening, model bytecode, near clipping and VGA painter execute; only model
handle resolution is supplied. Compact fixtures run without game data.

All **154 regressions** pass in development, Release and ASan/UBSan. Release
and sanitizers repeat 256 complete Gouraud-shadow frames each. All 128 retail
missions compose 64 frames without warnings; sanitizers repeat the corpus
for 16 complete frames each. Private-window controls still exercise MFDs,
weapons, cameras, designation, pause and real combat FINISH. The additional
`tools/check_window_shadows.py` observes a normal landed Tour mission reach
production silhouette drawing after F5 chase view, without injecting world
state. Earlier outer-transform sanitizer coverage also completed all 128
missions × 64 frames. Complete scene scheduling, immediate delivery and
model-local articulated transforms remain separate gates.


### Immediate submissions, persistent slots and draw-time state

SSD opcodes 4/6 now retain immediate delivery instead of entering the deferred
sort. Each draw request owns the model identity, orientation metadata and
eight parameter bytes **at its instruction**, before subsequent SSD state
changes. Both the world painter and arming preview use those snapshots.

Original `0xcbfec` uses the queue's current scratch slot without allocating
an entry. At capacity this can overwrite a deferred payload while retaining
its queued type and priority. The scene preserves that reuse, including a
later flush through the retained unrotated/scaled/shadow consumer. Size
wrappers mutate the cached coordinates on each actual draw, so immediate
painting followed by a flush can scale the same cache twice.

Slot metadata survives queue resets. Original `0xcc0bf` does not write its
size flag, so an unrotated model can inherit the previous flag in that slot;
the executable's initial 348 flags are all 255. Immediate constructors also
retain the prior global parameter bank, while deferred wrappers load their
copied eight bytes. The model's initial condition still uses its separately
submitted parameter 0. The renderer now owns these distinct slot and global
states explicitly rather than reconstructing them from the final SSD state.

`tools/check_instances.py` now compares delivery/orientation plus every draw's
model, size/orientation metadata and parameter snapshot across all **703 SSD
definitions / 19,589 persistent states**. `tools/check_model_delivery.py`
executes **256 original mixed sequences / 123,948 submissions / 111,174
delivered payloads**. Actual immediate/deferred/shadow constructors, slot
allocation/replacement, size wrappers, multiple frame resets and final sorts
execute. Submitted IDs, a common zero-bias header and initial slot/parameter
state are supplied boundaries; geometry header entries and final notification
are observed/no-op consumers. Records compare allocation/count, delivery
order, mutated cache, submitted and consumed parameter banks and Euler words.
Compact independent sequence hashes run without original files.

`tools/check_model_transforms.py --immediate` also executes the actual direct
constructor through **1,024 complete converted-model VGA frames**, agreeing
in all basis words and pixels. All **155 regressions** pass in development,
Release and ASan/UBSan; Release and sanitizers repeat 64 mixed sequences each,
and sanitizers repeat 256 complete immediate frames. All 128 retail scenes
compose 64 frames without warnings, and sanitizers repeat the corpus for
16 frames. Live weapon, MFD/camera, designation, pause and real combat FINISH
checks still pass. Front-end test cleanup now uses Shift+Q rather than treating
the faithfully restored Escape/ejection path as an immediate application exit.

These checks close the represented submission/ownership paths, not complete
renderer-phase scheduling or complete original sessions. Visibility-dependent
animation, model-local articulated transforms and rounded/textured boundaries
remain separate work.

The private front-end check also passes edited enrolment names, Control
qualification, DOS pilot save/reload, FWS briefing/custom arming/flight and
Tour squadron selection/route review/flight after these ownership changes.

### Visibility-gated animation and viewport jitter

The main player stream update (`0xb4f44`) now runs independently of visibility.
Ordinary SSD submissions (`0xb4ff0`) advance their streams only after their
bounding sphere is admitted; fixed player-pose submission (`0xb518c`) never
advances them. This also prevents hidden effect templates and previously seen,
now offscreen scenery from advancing merely because their instances exist.

The original animation gate remains enabled during instrument world passes.
Consequently an admitted ordinary model can advance again in a second viewport
within that frame. The reconstruction preserves this behavior, while consuming
the primary frame gate so extra host redraws do not advance model streams.
Queue reset (`0xc488d`) also saves/restores procedural jitter per viewport,
rather than once per physics update. Its initial seed remains 0x3141/0x5926.

The main camera's separate player submission precedes the world. Missile and
impact-orbit modes use the integer-coordinate, visible/animated constructor;
other external views use the Q8 fixed-pose, frozen constructor. Instrument
world passes do not submit that separate player model. A fully lowered
instrument viewport returns before resetting or drawing its scene.

`tools/check_animation_policy.py` agrees with **8,192 original model calls**
through all three real constructors, sphere queries and persistent stream
interpreters, including primary/all-eight streams and byte wrapping.
`tools/check_model_jitter.py` agrees with **16,384 original viewport resets**
with intervening draw consumption. Declared inputs are synthetic stream/source
bytes, initial state, camera/object poses, animation gates and working seeds;
the single instance resolver and final notification are supplied boundaries.
The separate vertex-jitter test still checks actual random consumption.
`tools/check_view_cameras.py --model-submission --refuelling --horizon
--projection --split-basis` agrees across **12,288 original camera updates**,
including the selected player constructor alongside all prior pose, matrix,
projection and horizon fields.

All 157 development and Release regressions pass. All 128 retail missions
compose 64 frames without warnings. The real factory collision/effect scene
still matches 32 original animation/drawing passes. Private-window checks
cover firing, MFDs, designation, pause, combat results, camera switching and
the authored seat-release/parachute/recovery sequence. These comparisons close
the represented constructor and viewport scheduling rules; complete renderer
phase order and full original-session equivalence remain separate gates.

### Native articulated model geometry and carried stores

Live model interpretation now retains the original Q15 camera/component basis
and wrapped dword vertex products. Rotated helper calls compose and quantise
the current camera basis before generating their vertices. Anchor helpers and
carried models use the already projected camera-space anchor, with native
translation/basis restoration and shared vertex slots. This replaces the
previous continuous local-matrix approximation; the independent inspection
path retains its continuous geometry.

Absolute, delta and jittered vertices now enter the same integer camera path.
Model camera-scale instructions retain their separate multiply/divide rules
and wrapped cached translations. The original component/view basis helper is
shared with the outer model transform. Existing queued, immediate and flattened
shadow consumers retain their own outer basis construction.

The live renderer also receives the original global carried-model table from
the current loadout. Presence follows the model's visible flag independently
of ammunition count, preserving the JP233 dispenser after its contents are
released. The table is shared by model interpretation as in the original,
rather than attaching a separate guessed loadout to every actor.

`tools/check_model_articulation.py` executes **1,022 complete original frames**
with two nested anchor/rotated helpers, carried-store entry, parent restoration,
absolute/delta vertices and multiply/divide/negative camera scales. All 27
submitted camera-coordinate dwords and all 64,000 VGA pixels agree per frame;
419 frames paint pixels. Two original IDIV faults have no completed frame and
are explicitly excluded, with a fresh capture CPU after each trapped input.
The original model converter generates the helper matrices from the disk
angles, and the actual queue, outer component generator, helper VM, projection,
clipping and raster execute. View/body bases, model/cache bytes and poses are
supplied inputs; resolving the two symbolic model handles is the only replaced
consumer. Compact original fixtures run without game files.

The existing outer-transform probe now uses this same camera-space interpreter
instead of applying an outer transform afterward. Another 256 ordinary frames
and 256 forced-flat Gouraud shadow frames agree with original pixels/bases.
All 158 development regressions pass, and all 128 retail missions compose
64 frames without warnings. Private-window controls still pass weapons, MFDs,
cameras, designation, pause and real combat results. A separate GDB observer
(`tools/check_window_carried_models.py`) confirms the normal training loadout
actually enters carried-weapon model execution in chase view without changing
world, inventory or result state.

These checks establish the represented helper/attachment arithmetic and live
binding, not all authored drawing opcodes, material-state sharing, complete
arming 3D-frame equivalence or full world/session scheduling. Rounded and
textured boundaries remain separate renderer work.

The articulated-model comparisons also pass for 256 complete frames each in
Release and ASan/UBSan, and all 158 regressions pass in both builds. The live
carried-model observer passes Eurofighter, F-22 and F-117 retail loadouts.
The independent inspection comparison still agrees on all 1,012 models /
2,042 detail/state cases / 253,334 vertices.

### Complete rounded-model surfaces

Native rounded quads now follow `0xbc660` rather than averaging their projected
screen corners. Each edge midpoint is formed from wrapped camera-coordinate
sums and projected separately; the centre is painted before the four original
`0xdaf50` corner fans. A rounded face with any near vertex is omitted as in the
original. Corner fans bypass the common raster's winding test, while the
central polygon retains it. Inspection keeps its separate continuous fallback.

Original `0xbc89f` has a retained aperture quirk: its midpoint shifts stay at
eight even after the missile-nose setter switches ordinary vertices to nine.
The reconstruction preserves that difference rather than making the rounded
shape internally consistent with a different result.

The full comparison also exposed two sparse/procedural painter rules absent
from the earlier face corpus. Sparse modes inspect skipped physical scanlines
and stop at the first inverted span. The procedural painter instead compares
VGA-byte width: an inverted pixel span inside one byte consumes a random mask
without painting. Both rules now follow their original consumers. Explicit
fixtures retain the three-pixel corner and random-state failures that revealed
them.

`tools/check_rounded_models.py` agrees across **1,024 complete original rounded
frames and final procedural seeds**, including 411 nonempty frames. Actual
projection, wrapped midpoint arithmetic, corner recurrence, winding bypass,
viewport clipping, integer edges and all nine represented patterned/logical
painters execute without geometry/raster substitutions. Camera vertices,
viewport, drawing mode and initial seed are supplied inputs. No native division
faults occur in this corpus. The compact fixture runs without game data.

Another **2,048 complete original ordinary/near-clipped patterned frames** still
agree after the shared painter changes. All **159 regressions** pass in
development, Release and ASan/UBSan; Release and sanitizers repeat 256 complete
rounded frames each. All 128 retail missions compose 64 frames without warnings,
and private-window controls still cover weapons, MFD/camera pages, designation,
pause and actual combat FINISH. The preceding articulated-renderer sanitizer
sweep also finished all 128 missions × 16 frames without warnings.

These are supplied model-face comparisons. Remaining material-state operations,
texture boundaries and complete world-phase/session equivalence remain open.

### Authored fixed-palette effects

Model opcode 84 (`0xbc9ac`) now switches subsequent colour lookups to the
original fixed row zero, rather than the submitted object's distance row.
This is used by explosion, flare/chaff and other authored effect programs.
The live painter receives a per-primitive palette source for flat, Gouraud,
textured and point/line consumers; forced black silhouettes retain their
separate path.

Flat ink remembers the table used when its colour was loaded. A palette switch
therefore does not recolour a later instruction that reuses that existing
ink. Newly loaded flat colours and interpolated Gouraud/texture indices use
the current table. The selection persists through model helper calls, matching
the original shared drawing state.

`tools/check_model_palettes.py` agrees across **1,024 complete original model
frames**, exercising seven switch positions, nested helper entry/return,
colour reuse, packed Gouraud shades and both apertures. Original resource
conversion, vertex generation/projection, model bytecode, clipping and VGA
raster execute without intercepted geometry or drawing helpers. Source model
bytes, identity basis/zero translation and two deliberately distinct lookup
rows are declared inputs. Compact original frame hashes run without game data.

All **160 regressions** pass in development, Release and ASan/UBSan. Release
and sanitizers repeat 256 complete original palette programs each. All 128
retail scenes compose 64 frames without warnings. Controlled private-window
land/water/contact crash scenarios still reach their selected models/cameras
and exit cleanly. `tools/check_window_crash.py --observe-fixed-palette` also
observes fixed-colour contact effects at the renderer boundary;
these scenarios deliberately seed a contact and do not claim unmodified
end-to-end original crash sessions. The preceding rounded-renderer sanitizer
sweep finished all 128 retail missions × 16 frames without warnings.

The separate parameter-selected ink/mode controls and specialised retained
helper opcodes remain distinct material work. General texture edge/near-plane
fidelity and complete renderer/session scheduling are still open.

### Original affine model textures

Unrounded textured faces now use the original integer camera projection,
near-plane clipping and affine texture raster. UV coordinates retain their
unsigned Q7 words through camera clipping and signed-word viewport clipping.
Edges use the original half-step bias; spans paint bottom first with wrapped
Q24 coordinates and palette-index transparency. The VGA loop's overwrite of
the low V-accumulator byte by the remapped texel is preserved too.

Flat, Gouraud and textured model consumers share camera-point recovery and
ordinary projection. Their near-plane attributes share the original bisection
and replay mechanism while retaining the distinct attribute widths. The
continuous inspection renderer keeps its separate fallback.

Original-executable comparisons establish:

- **1,024 complete screen-space textured frames**, including viewport edges,
  reversed/degenerate polygons, fractional UVs and transparent mapped texels.
- **512 complete ordinary and 512 near-clipped textured-model frames**,
  executing the actual resource converter, vertex generation/projection,
  global-atlas binding, texture dispatch, palette lookup and VGA raster.
- **8,192 near-plane UV intersections**, including full signed camera dwords,
  unsigned UV carries, both edge directions, wrapped midpoint replay and
  saturated ordinary/wide projection.
- **1,024 complete Gouraud near-clipped frames** unchanged after extracting the
  shared arithmetic.

The texture comparisons supply a diagnostic 256×256 atlas, model bytes,
lookup row and camera state. Only the requested atlas handle is resolved by
the harness; drawing and clipping execute unchanged. These are complete
supplied-model frames, not complete original world/session captures. Actual
retail atlases are 256×192; reads outside a supplied atlas are safely zero
extended rather than reading unrelated memory. No original division faults
occur in either 512-frame model corpus. Compact original fixtures run without
game data.

All **164 regressions** pass in development, Release and ASan/UBSan. Every one
of the 128 retail missions composes 64 development frames and 16 sanitizer
frames without warnings. `tools/check_window_textures.py` observes an actual
retail-atlas texel fetch in the live integer painter, selects the F10 canopy
view and exits normally, using private silent installs/X11 without changing
world state. That observer passes Eurofighter, F-22 and F-117 missions.
Release and sanitizers each repeat 128 ordinary and 128 near-clipped original
model frames, and sanitizers repeat all 8,192 UV intersections. Private-window
checks also pass all camera controls, MFD cycling, stores/cannon, designation,
pause and actual combat FINISH.

Parameter-selected ink/drawing controls, specialised lighting helpers and
complete world-phase/session equivalence remain separate work.

### Scoped polygon-depth palettes and crossed faces

Model opcode 118 now calls its relative helper with the original per-face
colour loader and re-used-colour handlers, restoring those handlers on return.
Each triangle or quad averages its signed camera-depth high words before
selecting a root colour-table row. Re-used ink gets a new average for each face;
the loaded ink/row survives helper return, while a later ordinary colour load
again uses the object's table. Opcode 84 still changes the shared table, even
inside that helper. Opcode 74 now selects the mutable distance used by later
model conditions from the chosen camera vertex, rather than being ignored.

The native comparison also exposed different crossed-face behaviours in the
painters. Flat spans are consumed bottom first through an unrolled VGA jump
table: its first negative byte-width entry skips a row, while wider inverted
spans terminate the face. Gouraud instead terminates at an inverted pixel span.
These distinctions matter for narrow clipped triangles and non-planar quads;
continuing all scanlines or applying one shared termination rule is incorrect.

Original comparisons now agree for **1,024 complete helper-palette frames**
(1,021 nonempty), **1,024 varied-depth flat frames**, **1,024 varied-depth
Gouraud frames** and **1,024 crossed/concave texture frames**. Actual resource
conversion, projection, conditional branches, helper/return dispatch, lookups
and VGA drawing execute. Camera/model inputs and deliberately distinct lookup
rows are supplied, not captured complete-world sessions.

Both retained models using opcode 118 use triangles, quads and triangle ink
reuse. The original variable-size-polygon form mistakenly includes its count
word as a vertex offset in this depth average. That unrepresented combination
now reports an explicit unsupported boundary rather than silently substituting
a corrected average; the current retained helper corpus does not use it.
Parameter-selected constant ink/drawing modes remain the next material gap.

All **168 regressions** pass in development, Release and ASan/UBSan. Release
and sanitizers each additionally agree on 160 complete original helper-palette
frames and 160 varied-depth Gouraud frames. Explicit fixtures retain the exact
narrow-triangle, crossed-quad and Gouraud failures found during this work.
All 128 retail scenes compose 64 development frames and 16 sanitizer frames
without warnings. Private-window checks retain all three MFDs, weapon release,
missile/cannon views, designation, pause and actual combat/result handoff.

### Native material state and complete rounded instructions

Opcodes 86/87 now select raw constant ink and a drawing mode from the original
parameter bank. Opcodes 88/89 restore their respective consumers. The loaded
ink and the raw source used for later depth re-shading are separate: restoring
normal loading does not retroactively replace the already loaded constant.
Inline colour lookups used by rounded surfaces also retain their distinct
source-ink behaviour. Line and sized-point colour loading now follows those
separate native stores.

Changing the base Gouraud/texture consumers is not always immediate. The
original projector updates their current callbacks when a near vertex occurs;
otherwise the existing consumers remain selected. The interpreter retains this
distinction, including constant-flat drawing after the appropriate projection
and ordinary shaded/textured drawing before it.

`render::paint_model_polygon` is now the common native dispatch for both live
world drawing and supplied-model frame checks. Plain rounded quads (opcode 56)
previously reached the ordinary flat path; they now use their original curved
consumer, like explicit-mode opcode 62. Standalone arc instructions 90/91 use
fourteen points, while quad corners use eight. Both share the integer recurrence
with its original constant and shifts. Rounded entry facing retains its mixed
dword/word difference test; near corners omit the entire rounded surface.

The complete programs exposed additional painter distinctions. Dense stipple
and logical modes continue past an inverted pixel span within one VGA byte,
but end on an inverted byte width. Sparse modes distinguish selected rows from
skipped rows, including their initial special cases. The flat unrolled painter
retains its separate first-negative-byte skip. Treating all these cases as an
empty pixel span drops real curve-edge pixels. Explicit model fixtures retain
the failures found at cases 25, 305 and 333.

Original-executable evidence includes:

- **512 complete synthetic material-model frames**, including constant ink,
  parameter modes, restoration, reused ink, ordinary helpers, fixed palettes,
  Gouraud/texture consumer selection and final procedural seeds.
- **1,024 complete rounded-model programs**, exercising 56/62 and 90/91,
  explicit/global modes, retained ink, both apertures, near omission and
  overlapping primitives; 725 produce nonempty frames.
- **4,096 direct supplied-span frames**, checking all ten represented flat,
  stipple and logical consumers on uniform and patterned backgrounds. Factories,
  stack consumption, hardware IO, VGA pixels and final seeds execute unchanged.
- **128 frames of the retained `russ` model**, plus 64 each of the two retained
  depth-shaded models. Their original resource bytes remain external; fixtures
  contain synthetic programs only.
- **1,024 varied-depth patterned frames** and 256 existing rounded-kernel frames
  retain their original results.

Only diagnostic texture-handle resolution is substituted in the complete-model
harness. Camera state, parameters, model bytes, lookup rows, initial procedural
state and atlas are declared inputs. These comparisons do not establish a
complete original world/session capture, all factory modes outside the ten
represented consumers, or the older sprite-backed model path.

All **172 regressions** pass in development, Release and ASan/UBSan. Release
adds 128 original material frames and 384 rounded programs; sanitizers add
1,024 supplied-span frames and 384 rounded programs. All 128 retail scenes
compose 64 development frames and 16 sanitizer frames without warnings.
Private-window controls still pass MFDs, stores/cannon, designation, pause and
actual combat/result handoff.

The earlier independent JavaScript inspection comparison predates the newly
executed opcode-118 helpers and retained-ink distinctions; it is not the oracle
for these changes. Complete original VGA frames provide the stronger evidence
here. Remaining world-renderer work includes point/line precision, older
sprite-backed model drawing and complete phase/session scheduling.


All three controlled live land/water/contact crash scenarios also pass. The
fixed-palette observer now targets the actual renderer boundary and the contact
model (`kapow`), whose program explicitly selects row zero. The land `crash`
model does not contain opcode 84, so requiring that branch there was an invalid
assertion. When observing this hot path, the contact scenario advances complete
fixed-tick frames before opening its window; debugger latency cannot age the
scenario's external real-time clock. These remain explicitly seeded integration
checks, rather than captured original gameplay sessions.

The controlled land/water/contact crash checks also pass under ASan/UBSan,
and a live F-117 check still observes actual retail-atlas texels in the integer
painter after dispatch extraction. Standalone verification scripts retain
executable permissions consistently; imported helpers remain separate.

### Authored points and particle discs

`render::paint_model_primitive` now shares native dispatch between live worlds
and complete supplied-model frame checks. The polygon consumer is unchanged;
authored one/two-pixel points and sized discs no longer fall through the
continuous inspection renderer. The separate distant-model point path retains
its independently verified rules.

Opcodes 51/52 use stored signed-word projected coordinates, high-word near
eligibility and the current normal/wide projection. One-pixel anchors include
viewport edges; two-pixel anchors exclude all four edges. They retain their own
palette lookup even when constant polygon ink or a global drawing mode is
selected. Neither introduces a per-vertex far cutoff beyond parent admission.

Opcodes 92/93/95/96 retain signed radius-word multiplication and high-depth
integer division. Radius scaling remains unchanged in the wider missile-nose
view, although the centre projection changes. Plain discs inherit the active
global mode; explicit disc modes override it temporarily. Constant ink, fixed
palette selection, procedural masks and final random state reach the shared
native span consumers. Zero/negative/100-or-larger projected radii draw nothing.
All 4,950 original circle extents agree with midpoint-circle construction, so
the native world painter generates them without embedding executable bytes.

The original partial-circle path has a visible corner case: it omits empty
horizontal spans, decrements its lower endpoint for omissions at/after the
centre, and only then clamps that endpoint to the viewport. Crossing two edges
can consequently shift the painted rows. Complete program case 189 exposed
this; the retained synthetic fixture preserves it.

`tools/check_model_dots.py` executes the original converter, model program,
projection, palette/material state, circle builder and VGA callbacks without
substituting drawing helpers. It compares **1,024 authored-dot frames** and
**3,858 completed disc frames** from 4,096 cases; 238 native word-division faults
have no completed frame and are explicitly excluded. Inputs include signed
projection-origin wrapping, fractional near depths, every native circle
radius, partial cockpit/MFD rectangles, both apertures and all ten represented
flat/pattern/logical consumers. The compact regressions contain synthetic
programs and frame/seed hashes, with no original resource bytes.

All **174 regressions** pass in development, Release and ASan/UBSan. Release
and sanitizers additionally compare 192 original dot frames and 256 disc
frames. Every retail mission composes 64 development frames without warnings;
private-window checks pass live missile smoke, all MFD controls, weapon and
missile views, ground designation and an actual combat/result handoff.
These supplied-input comparisons do not establish complete original session
scheduling or the older sprite-backed model path. Native lines remain next.

The complete retail scene corpus also passes 16 sanitizer frames per mission
without warnings after the particle dispatch change.

### Original laser-designator camera and overlay

The laser designator no longer uses the continuous inspection camera or
placeholder crosshairs/ZOOM text. It selects viewport 19, with the original
159/99 projection origin, full-height bounds and normal aperture. Search and
tracking use the original integer camera cache and Q15 view matrix. The cache
rotates each full position dword by sixteen bits; a plain left shift would
lose its retained fractional bits above the world-word boundary.

The authored `targcros.spr` marker follows the actual projected ground point.
Original T/L labels use ink 16 and independent two-draw search/tracking phases:
search flashes T and retains L, while tracking retains T and flashes L.
Sprite arguments zero-extend the projected words; label offsets sign-extend
their resulting words. A failed (-1,-1) projection consequently hides the
sprite, rather than clipping a stray marker into the upper-left corner.
The first designation frame retains its search presentation; subsequent
tracking follows the selected point. Native view selection, joystick view
buttons, automatic landing and ejection/loss transitions leave the designator.

`tools/check_designator.py` compares **1,024 complete original search/tracking
controller updates**, with 595 nonempty overlays. Original gimbals, zoom,
integer camera setter, view matrix setter, reciprocal target projection,
sprite painter, glyph painter, blink stores and redraw flag execute. Supplied
inputs include keyboard states, inherited gimbal/zoom state, player pose,
designated Q8 points and projection origins. Each complete 64,000-pixel image,
coverage image and retained camera/indicator state agrees.

Only world/horizon drawing, VGA batching and resource lookup are external
boundaries: the capture supplies a uniform previously drawn background and
the exact original marker bank. Horizon pitch/roll/height arguments are
observed, not replaced with C++ values; the horizon painter has its separate
native comparisons. These are complete controller/overlay comparisons, not
complete original world frames or acquisition/session captures. Ninety-five
compact camera/projection fixtures are asset-free and run in the regular suite.

All **175 regressions** pass in development and Release. Release additionally
compares 256 complete original designator updates. Every retail mission
composes 64 development frames without warnings. A private silent window
observes actual search/zoom camera and search/tracking display calls using
normal keys, then verifies unlock, F1 cockpit return, pause and clean exit;
no target, controller or outcome state is injected. Broader private-window
checks retain MFDs, weapon/cannon views, ground release and real FINISH handoff.

The **175-test** suite also passes ASan/UBSan, with another 256 complete original
designator controller/overlay comparisons. All 128 retail missions pass 16
sanitizer frames without warnings. The live observer additionally enters the
designator again and ejects through normal Escape input, confirming that the
real ejection camera replaces it; both development and sanitizer builds pass.

### Retained inert model instructions

Opcodes 6 and 80 are literal no-ops in the reference handlers at BBFA6 and
BC8D9: they consume six and three operands respectively, then dispatch the
next instruction. They neither draw nor change material state. They now have
explicit interpreter cases, rather than an unsupported-context warning or an
attempted polygon. The retained F-117 arming program contains opcode 6.

`tools/check_model_materials.py --inert` inserts both instructions between
stateful drawing operations. **512 complete original model programs** agree
in every pixel and final procedural seed, including constant/fixed
palette state, helper calls, ink reuse, restoration, shading and textures;
425 produce nonempty frames. Compact synthetic programs retain this evidence
without original resource bytes. All **176 regressions** pass in development,
Release and ASan/UBSan; Release and sanitizers each additionally agree on 128
complete original programs with these instructions.

### Mission-start cockpit and weapon selection

A complete original mission 501 launch exposed two later initializers whose
results were lost when the individually checked subsystems were assembled.
`0x7df30` initially lowers the panel by 28 pixels, but the environment branch
at `0x71280` raises it to zero for the later sky renderer. All 33 decoded TFX
environments select that branch. The flight host now applies that final offset
on launch and restart; Page Up/Page Down still operate the original movement.
The environment-binding comparison checks this flag against the complete
native parser alongside its palette, layer, night and sun bindings.

The command initializer at `0x83cb0` also runs after the automatic pylon
selection and enables the weapon sight. Flight now starts with the aircraft's
air-to-air or air-to-ground sight instead of forcing navigation mode. This
retains the selected pylon and the native cannon sentinel behaviour.

All 176 CTests pass in development, Release and ASan/UBSan builds. A private
window/input check also exercises designator selection, zoom, tracking,
cockpit return and ejection after the offset change. The arming capture's
mouse transport now writes the original WORD outputs rather than DWORDs;
3,072 controller sequences, 7,241 hit tests, 274 complete widget/panel frames
and 81 fade-palette frames continue to agree.

### Complete flight capture, retained ground reference and terrain selection

```sh
python3 tools/check_flight_session.py --build build --frames 256
python3 tools/capture_native_flight.py --output /tmp/tfx-original --frames 4
python3 tools/check_terrain_cells.py --build build
python3 tools/check_terrain_variants.py --build build
python3 tools/check_world.py --build build --research ../tfx-reverse --terrain-only
```

The session comparison launches the genuine C++ executable under read-only GDB
observation in a private installation. Its counterpart runs the original
`0x72588` with real configuration, EMM/renderer startup, environment/resources,
DEFAULT/DONE arming actions, AI and complete frame bodies. Planar VGA preserves
the original three drawing pages and fourth backing page. Original allocators
run inside bounded host storage; only the external Watcom allocation boundary
is replaced. Inherited presentation declarations and DOS I/O are documented
inputs in the capture's JSON dependency record. Capture images remain outside
the repository. `--strict-pixels` additionally requires zero image differences;
it checks every captured frame rather than only the final screenshot.

The full launch exposed sequencing that isolated component tests did not:

* The native ground-reference position survives camera/mission resets. It now
  shares the application lifetime of the retained horizon counter. Ordinary
  flight first advances simulation and then draws; an extra unadvanced startup
  frame no longer consumes camera, HUD and scenery state.
* Ordinary TFX environments visit a 16×16 terrain neighbourhood. The inspector's
  radius is separate. **512 complete original traversals / 120,564 submissions**
  verify Z-major ordering, exclusive upper bounds, narrow world centres and
  map-edge wrapping. The original negative-X seam adjustment happens only at
  the start of each row; subsequent columns increment and wrap normally.
* File layout is column-major, but `0xb0708` assigns random scenery variants
  by **stored row, then column**. The old inspection sequence used file order;
  it reached the same RNG state but chose different scenery. The complete
  native initializer now verifies **1,440,000 choices/rotations across all 36
  retained grid/environment combinations**, as well as each final RNG state.
  The research viewer and collision comparison use the corrected order too.
* Native model admission limits object distance, but its admitted faces have
  no additional per-vertex far cutoff. **256 complete original material-model
  frames** verify vertices beyond the inspection far plane, with every pixel
  and procedural seed checked; Release and sanitizers each add 128 cases.

The older static SSD-declaration summary in `check_world.py` differs from the
newer runtime interpreter on conditional draw lists. `--terrain-only` isolates
its terrain metadata comparison; runtime SSD behaviour remains checked against
original execution by `check_instances.py`. After the variant correction,
**4,800 complete original terrain collision queries** still agree, including
seams and all quarter turns. Private-window control and designator checks pass.


### Authored model lines and HUD frame order

Model opcode 1 now uses the original Q16 projection, near-plane intersection,
signed-word screen coordinates and clipped integer line painter. It no longer
falls through to the inspector's floating projection and general clipping.
`check_model_materials.py --lines --cases 2048` compares complete original
models, including ordinary and wide apertures, coincident endpoints, near and
viewport clipping, fixed/distance ink and mode changes: **2,048 cases agree**,
including 546 visible lines, every VGA pixel and procedural seed. Compact
synthetic captures form an asset-free regression. The material probe now
retains its normal inspection far distance, so the existing admitted-face
regression actually detects the removed per-vertex cutoff; 256 complete
original far-face cases still agree.

The every-frame session check exposed two host ordering errors. Original
`0x72324` advances mission time before the HUD, while `0x70e44` dispatches
scenery events afterwards. The timeline now separates these operations; its
4,096 native boundary cases still cover equality, ordering and unsigned wrap.
Previously the cockpit clock lagged at each second boundary. Original
`0x7213f` also updates the seeker before the working forward HUD scans contacts
at `0x8015b`: a new contact becomes available to acquisition on the following
frame. Scanning now happens at that drawing boundary, preserving HUD failure
and external-view eligibility. Simple/physical G-load readings are supplied
from their retained flight state instead of an Arcade-only zero.

```sh
python3 tools/check_flight_session.py --build build --frames 32 --strict-pixels
python3 tools/check_flight_session.py --build build --flight-model simple --frames 32
python3 tools/check_model_materials.py --build build --lines --cases 2048
```


### Advanced-flight HUD instruments

The forward HUD now draws the original angle-of-attack tape and climb-rate
indicator in Military-spec and Realistic flight (`0x7f870`, `0x7e130`,
`0x7e328`). It uses the retained physical alpha and NED down velocity, original
monochrome artwork, approximate-pi conversion, truncating integer/word stores,
clamped climb pointer, feet-per-second readout and separate line/glyph clipping.
The climb tape retains the original direct linear VGA addressing at the right
edge rather than treating it as a clipped polygon.

`check_hud_text.py --aerodynamic` compares **308 complete original indexed and
coverage frames**, spanning alpha/climb thresholds, truncation neighbours,
negative climb and cockpit offsets. The original pixel writer has no endpoint
bounds check; coordinates that access unrelated memory are explicitly excluded.
The pre-existing 768 flight-readout/message frames still agree. Complete
mission-501 comparisons now match every pixel and retained observed state
through 32 frames in both advanced flight models. All 179 regressions pass
in development, Release and ASan/UBSan.


### Moving-actor draw admission

Original `0x78180` admits aircraft within 16,384 world units of the current
camera anchor on X and Z; `0x7c354` also tests Y for surface units. These
inclusive Q8 tests precede sphere projection and model LOD. Applying only the
renderer test incorrectly showed distant aircraft, including a single MIG-21
pixel in the training comparison. The main camera and instrument scene passes
now select actor visibility from their own anchor before drawing.

`check_actor_visibility.py` runs both complete original pool walkers for
**4,096 position/reference pairs**, recording admitted SSD handles at the
orientation/drawing boundary. Threshold neighbours, altitude eligibility,
subtraction wrap and the original negative INT_MIN absolute-value result agree.
Compact synthetic inputs/decisions run without game data. All **180 development
CTests** pass; the complete Simple-flight comparison agrees on every indexed
pixel and observed state for **64 frames**.


### Landed startup and the first player callback

Original `0x8c184`/`0x8c318` initialize flight before the first frame, but the
runway setback or carrier offset belongs to `0x8f56c..0x8f686`, at the beginning
of the first player callback. Startup and placement are now separate operations.
The player owner consumes its pending cold start once before ordinary motion;
subsystem probes can still check both boundaries explicitly. Player-SSD setup
`0x7d736` separately seeds landed integer Y to -3, without changing float Y.
Previously the eager placement made the initial retained pose disagree even
though the first completed flight pose was correct.

`check_aerodynamic_launch.py --all-models` and `--before-placement` each verify
**1,536 complete original startups / all 316 observed flight/control fields**.
The original **4,608 complete player callbacks / 48 warm model-changing scenes**
also agree in all 324 observed fields. The complete F-117 mission-508 capture
now matches its initial and subsequent player poses, actors, RNG and camera
through 16 frames; remaining image differences are investigated separately.

The complete direct-launch capture records its explicit inherited presentation
numbers in JSON. Arcade scoring/permission eligibility matches the C++ direct
`--mission` runner independently of the numerical flight model. Radio calls are
now observed in the original JSON and read-only C++ GDB log, rather than being
inferred from the displayed characters alone.


### AWACS contact reports and launch radio

The working forward HUD's radar scan (`0x836ac`) now retains the original
nearby-enemy count and reports increases within 25 miles, separately from its
50-mile contact admission. It selects the original farthest qualifying aircraft,
delivers the idle-only ticker notice, queues urgent spoken bearing/altitude and
requests flight music cue 2. The count survives mission changes as DATA 0x7774
does; it is not a new-mission reset. Landed starts also issue the original
`CLEARED FOR TAKEOFF` call from `0x7df30`. Radio advancement follows the scan,
so an announcement participates in the current frame rather than one frame late.

`check_sensors.py --announcements` compares 1,024 persistent original radar
frames and 4,096 seeker states, including announcement count, eligibility and
chosen actor. `check_radar_voice.py` additionally executes the complete native
`0xa4e80` numeric builders: **4,096 sample sequences agree**, covering guards,
zero, teens/tens, hundreds, lookup-table tail and urgent submission. Its compact
fixture contains sample-slot numbers, not recordings. The original overlapping
number lookup is preserved; inputs that index beyond its defined storage are
not presented as verified behaviour.

Whole-session comparison now checks actual ordered radio requests, including
calls before frame 1, as well as every retained state and image. The F-22's
mission-601 Arcade run agrees on all observed state, radio and every indexed
pixel through 16 frames. Mission 501's 256-frame run agrees on player, AI,
random, camera and radio state; remaining image differences are investigated
independently. `--native-capture DIRECTORY` reuses a matching immutable original
capture for C++ reruns after checking mission, flight model, inherited campaign
state and duration; fresh original capture remains the default.

### Sky submission and scenery ties

The original sun/lightning model enters the bounded drawing queue before
terrain and moving objects. Previously it was submitted at the end. Even an
offscreen sun affects quicksort's equal-priority order, causing visible ground
polygons to cover one another differently. A complete native/C++ submission
trace identified the displaced entry; the production scene now submits queued
sky models first. This removes the persistent 13-pixel runway-edge discrepancy
in the landed F-117 capture without changing polygon rasterisation. Through
16 frames, 14 F-117 images now agree exactly; two retain small animated-scenery
differences under investigation.


### Retained scenery animation timing

The complete F-117 trace exposed a host-order error in animation, despite the
isolated clock and stream interpreters already agreeing. Original `0x7205c`
adds the **previous** frame's elapsed time before player input obtains the new
interval; `0x7202f` seeds the first interval to five ticks. Flight now advances
the scenery gate at that boundary. Its accumulated remainder is application
state, like original DATA 0xe74c, whose only consumers/writes are in that gate;
starting another mission retains it. Independent scene inspectors keep their
own clock. Complete-session captures now expose and compare each gate/remainder
instead of relying only on the resulting pixels.

The preceding AWACS/sky integration passes all **181 asset-free regressions**
in development, Release and ASan/UBSan.


The corrected gate makes all 16 mission-508 F-117 indexed frames agree exactly,
alongside the retained player/AI/RNG/radio/camera state and explicit animation
fields. The remaining mission-501 pixel was independently traced to its actual
native writer: an aircraft's distant **shadow**, not the aircraft body or a
scenery raster edge. The original shadow constructor retains the coloured
one/two-pixel LOD before its forced-black polygon path. The host incorrectly
suppressed those distant shadow representations. They now follow the ordinary
native distant-point painter.


With both host corrections, **all 256 mission-501 Arcade frames now agree in
every indexed pixel**, as well as player/AI, shared RNG, radio, camera and
horizon state. The landed F-117's 16-frame exact comparison also remains
unchanged. These are complete launch/frame-loop comparisons, not separately
constructed renderer scenes.


### Target-cue axes, seeker startup and authored shadow pixels

A broader complete-session matrix exposed an off-sight cue reflected across the
HUD. Numerical flight was already correct: the painter transposed its logical
world-space aircraft axes again. The old isolated probe also read native
row-major words as axis vectors, masking that integration error. Both probe
adapters now decode the native storage explicitly; production uses the actual
body axes, including Simple's separate body/display attitudes. The unchanged
native input corpus still agrees on **1,600 complete target-display frames**
and **1,024 composed forward-HUD frames** with warnings, refuelling and ILS.

Original mission setup calls `0x930d4` after arming. It sets the lock countdown
to 500, clears acquisition/actor pointers and seeds the reticle from the current
renderer aperture, retaining the projected target and too-close observation.
The final weapon thumbnail supplies that aperture after arming; missions using
DEFAULT_WEAP retain the prior aperture instead. Live startup now follows this
handoff, deriving the thumbnail centre from the decoded original rectangles.
The retained seeker observations and projection origin have application lifetime.
An additional **1,024 complete native startup resets** verify signed aperture
coordinates, countdown, cleared pointers and intentionally retained fields.
Compact cases extend the existing sensor fixture rather than duplicating it.

Complete-session captures now additionally compare the real body matrix, radar
slots, selected/acquiring/locked actor identities, reticle/projected coordinates,
lock timers and too-close state. They record the preflight aperture and original
executable/archive SHA-256 values; capture reuse checks those hashes when present.
Fresh Colombian and F-22 sessions agree on all these fields and every pixel.
All nine selected 16-frame regional sessions also agree in every indexed pixel:
missions 503, 504, 506, 605, 703, 745, 801, 922 and 965, spanning carrier/runway,
airborne, day/night and storm contexts.

The Libyan trace exposed a distinct shadow rule: authored opcode 51/52 pixels
retain their mapped colour, whereas polygons, lines and discs use shadow ink.
The live scene preserves that distinction. `check_model_transforms.py --shadow
--effects` executes the original converter, queued shadow constructor, matrix
composition, bytecode and VGA painters for **1,023 completed frames**, including
711 nonempty images. Every pixel and all nine matrix words agree; one original
IDIV-overflow input has no completed frame and is excluded explicitly. Its
compact synthetic regression adds no original model bytes.


### Suspension bump and player/cockpit ordering

The aerodynamic ground solver already emitted the original suspension-bump
request, but the host discarded it. Its distinct `0x7d9d4` cockpit callback now
runs after player physics/cannon and before timed messages (`0x720b8`), on the
requesting frame. Later flak requests still begin on the following frame.
The callback shares the flak cursor but tests a different sentinel table;
its ordinary two-step movement is +4 then -4 pixels. Neither a view change nor
its completion restores the separate saved flak offset.

`check_cockpit_shake.py` now includes **8,192 mixed original callbacks** with
suspension requests, flak, panel movement, interruptions and wrap boundaries.
The injected suspension cases stay within the declared table prefix; walking
past the original tables into unrelated bitmap globals is not presented as
verified behaviour. All compared offsets, cursors and callbacks agree.

The complete flight-model matrix covers **32 frames in each of the four
models**, both airborne mission 501 and runway mission 506. Every indexed
pixel, player/body pose, actor, shared random state, camera, radio, seeker and
scenery clock agrees in all eight sessions. This includes the naturally reached
Military-spec suspension bump and its same-frame projection/HUD movement.

All **182 asset-free regressions** pass in development, Release and ASan/UBSan.
After the ordering correction, both the eight-session flight-model matrix and
nine-session regional matrix were rerun and remain pixel/state exact. All
**128 retail missions** also complete 64 fully composed C++ frames with no
reported preview limitations. These short corpus runs establish startup/frame
coverage, not successful completion of every mission.


### Accelerated time and operating-area enforcement

The live `T` key now drives original `0x851dc` and the `0x86898` request/gate
semantics. Requested acceleration survives temporary steering and automatic-
landing inhibition; the effective flag reaches the existing flight, fuel and
radio consumers. The clock can refresh those settings during a pending poll
without discarding its accumulated ticks. Airborne world time runs fivefold;
landed world time and the physics timestep retain their separate native gates.
Cancelling marks the ticker idle while retaining its history, cursor and repeat
state. The native persistent-message wrapper is observed as one public request,
without double-counting its call to the ordinary builder. Completion/protection
notices now use that repeating delivery too, as original `0x719a4` does.

`check_acceleration.py` executes **2,048 original input/controller cases** with
all six steering inhibition words and autopilot modes, comparing requested and
effective flags plus retained radio state. Its numerical player callback is a
declared output boundary. The existing **4,096 clock sequences**, **4,096 radio
updates** and **512 complete radio pixel/coverage frames** remain exact; radio
checks now include stop followed by replay. A separate private X11 check presses
the real keys and observes the actual player boundary, without changing game
state: airborne/runway launch, T toggle, held steering, automatic resumption and
the separate world/physics intervals.

Original `0x71d74` operating-area enforcement is also restored. It retains signed
WORD cell conversion, the safe area and outer warning band, idle-only warning,
repeating departure notice, once-per-departure orders penalty and 810-tick
return countdown. Returning to the safe area permits another departure penalty.
Active acceleration is cancelled before the warning, which itself can make an
idle-only warning eligible. Target notices precede this handler; completion and
return checks follow it. **4,096 complete original boundary calls** agree,
including negative coordinates, quotient/WORD wrap, exact limits, prior
penalties/countdowns and radio/acceleration combinations. Compact fixtures keep
synthetic inputs and state only.

All **184 regressions** pass in development, Release and ASan/UBSan. The private
key/timing check passes in **all four flight models** (189 observed player
callbacks across the eight airborne/runway sessions). The unchanged 32-frame
Military-spec runway session still matches every original indexed pixel and
observed player/actor/sensor/random/camera/radio/scenery state.


### Stores jettison and a successful campaign engagement

`Alt+J` now executes the original `0x85118` pylon walk: ground-role stores and
external tanks release once per occupied slot in ascending order. Ordinary
release/selection semantics, mutable cannon role, target clearing, unlimited-
store behaviour and bay events remain intact. Launch sounds are suppressed,
and fuel above the internal capacity is discarded even when no tank releases.
The projectile constructor selects the native dropped-store stage program,
which is already shared by the external tank and submunition definitions.
Projectiles borrow that immutable sequence from the runtime's definition owner;
their weapon identity, geometry, coefficients and original stage transitions
remain separate. No replacement flight path is invented.

`check_jettison.py` executes **4,096 complete original handler cases**, comparing
guards, Alt eligibility, ordered transformed launch geometry/targets, inventory,
visibility, selection, release mode and exact fuel bits. Projectile construction
is its declared output boundary. `check_projectile_runtime.py --jettison-only`
independently executes the original constructor, linked population, all motion
stages, retirement and callback ordering for **644 frames**, at six timestep
sizes, for all 27 weapon identities. Every retained field, output event and
random state agrees. The existing **6,192 normal pylon/release cases** also agree.

`check_training_engagement.py` now provides a complete successful flight check
using ordinary window inputs on an isolated X11 display. Read-only GDB observes
targeting and results: select AMRAAM, accelerate time, wait for acquisition and
lock, fire, reach the real MiG loss/completion and execute FINISH. The direct
run completes after 507 observed frames. `--campaign` additionally restores a
disposable DOS-format pilot before Hop 1, uses the original DEFAULT/DONE arming
controls, follows the successful debrief and enters Hop 2 with the actual kill,
score and mission-count variables. That run completes the engagement after
434 observed frames; its real timer means the count is not a golden constant.
No damage, result, hit or mission-completion value is injected. FWS's original
SPOS belongs to training graduation, so the check verifies the pre-hop pilot
file remains unchanged instead of inventing a per-hop autosave.

The complete input-driven comparison exposed a separate host omission: native
store release retains its inventory when `arcade != 0 && simulator == 0`. That
rule now reaches both ordinary release and jettison, independently of numerical
flight-model selection. The native last-release mount also has application
lifetime and starts at zero; standalone subsystem probes still supply their
explicit initial sentinel. In the eight-frame mission-502 comparison, ordinary
J is inert and Alt+J agrees on player/actor/random/camera/radio/scenery, complete
seeker/radar state, selection/inventory/fuel and every captured projectile field.
Projectile drawing differences remain under investigation; numerical agreement
is not presented as whole-image equivalence. All **185 regressions** pass in
development, Release and ASan/UBSan.


### Input-driven complete-frame comparisons

`check_flight_session.py --inputs INPUT.json` now runs the actual window loop on
an isolated X11 display. The shared input file declares per-frame key events
and held keys; omitted frames are neutral. GDB supplies only external GLFW
keyboard, timer, focus and mouse return values. The relocated original receives
the same DOS key events and held-key array. No player, actor, random, weapon,
mission outcome or game instruction is replaced. Resource hashes, inherited
Arcade/simulator values, numerical model and four-tick intervals are checked
before comparing captures. Reusable native captures must match these inputs.

The observer also compares live pylon inventory, selection, last-release mount,
mutable cannon role, exact fuel bits and the original linked projectile pool,
in addition to the existing player, actors, shared random stream, body basis,
sensors, camera, radio, animation and indexed frames. Smoke sample transport is
observed on both sides without replacing its producer or pool. Any suppressed
scene diagnostic fails the comparison. The original unused cleared-shot read
at flat address `0x4b` is supplied by an explicitly zeroed DOS low page, matching
the already declared terrain-credit harness boundary.

This exposed several caller-order differences that isolated arithmetic checks
could not reveal:

- The HUD chooses NAV from the gun/cannon flags, then AA/AG from the **current**
  selected role. Automatic release and jettison can change the role without a
  selection key. Tank-only roles retain the common framing without an AA/AG
  sight. Seeker suppression now reaches the live sight too.
- The original ground HUD paints the mission target's projected cross when in
  range, and always paints its six-line direction cue. This is separate from
  laser designation and its rectangle. The full original marker is compared
  alongside the other target displays in **1,920 pixel/coverage cases**; the
  NAV/AA/AG/tank framing passes **1,024 composed original frames**.
- Projectiles submit newest first, including their final spent frame, with a
  strict 10,000-unit draw range and camera-relative SSD distance queries. They
  enter the bounded model queue before other moving actors. Ordinary actor
  queries continue to use the player; standalone inspection uses the camera.
- Smoke cleanup follows the submitted SSD animation. Checking destroyed flags
  while merely collecting deferred submissions retained an expired model for
  another frame, producing conspicuous excess trails during accelerated time.

The eight-frame mission-502 J/Alt+J comparison now agrees in **every captured
state and every indexed pixel**. The 160-frame mission-501 accelerated AMRAAM
run agrees on all captured state, projectile motion and emitted smoke samples;
155 complete indexed frames agree exactly. Five launch frames currently differ
by two HUD pixels each; the final frame agrees exactly. Those residual writes were subsequently identified as the launch notice
described below; numerical agreement alone was not treated as whole-image
equivalence.


All **185 regressions** pass in development, Release and ASan/UBSan after these
changes. The private-window training check also completes a natural AMRAAM kill,
objective/FINISH, original debrief and the next training hop with the actual
campaign score/kill counters; the pre-graduation pilot file remains unchanged.


### Release feedback, launch notices and final-frame retirement

The remaining launch-frame discrepancy above was the original **LNCH** notice,
not a raster error: `0x84052` arms five **AA HUD paints**. `0x7e8fa` consumes
one per AA layout, replacing LOCK/ARM; NAV, AG, failed HUD and external views
retain the counter. The message comes from executable DATA `0x16fc`. Trigger
eligibility sets it independently of whether the release primitive ultimately
accepts a shot. The composed targeting check now exercises this retained
counter alongside alerts, refuelling and ILS in **1,024 complete original
pixel/coverage/state frames**. All **160 indexed frames** of the controlled
mission-501 accelerated AMRAAM session now agree exactly, together with every
previously captured numerical field and smoke sample.

Ordinary store release now delivers the original normal-priority two-sample
sequence before the bay event and projectile construction: slot 11 (`fx16`),
then slot 64 (`bombs`) for categories 3/5/7/9, otherwise slot 73 (`missile`).
The non-Eurofighter player receives SSD entry 5, including during silent
jettison. `check_store_feedback.py` executes **3,072 complete original caller
cases** across every category byte, all aircraft, jettison and inhibition;
a compact reference fixture checks the sample selection without game assets.
Optional external-control-device output is still outside that playback path.

Refuelling saves/restores native `0x7140`, the weapon/gun flag, rather than the
three-way HUD layout enum. The cannon NAV preference (`0x7148`) stays separate.
The complete eight-frame AG-selection/refuelling-on/off session agrees on all
captured state and every indexed pixel.

The full observer now also reads player SSD parameter bytes and observes the
frame **after** native `0xa31a0` / C++ `runtime::finish_frame`. Two further
caller omissions were exposed:

- Landed startup installs SSD entry 25 before the loss/reset pair 21/0 at
  `0x7d720..0x7d7b8`. The gear stream then reaches its authored state at the
  normal animation gate, even while the cockpit hides the external model.
- Spent projectiles survive their final drawing pass, then retire after the
  flip. Waiting until the next motion update left stale target references
  visible to the intervening input/AI stage. A zero stage timer alone still
  waits for the next motion pass; it does not qualify for frame-tail removal.

Complete eight-frame F-117 (mission 508) and F-22 (mission 504) J/Alt+J sessions
now agree on every captured state, every player model parameter and every
indexed pixel, including a store that hits the ground in its release frame.
`check_projectile_runtime.py --frame-tail` adds **6,440 complete original
population frames**, with declared deterministic contact losses, all ten
population scenarios, child births, decoys, multiple substeps, retirement,
shared RNG and borrowed warning records. The legacy `0x75030` visual output
is an explicit boundary in that synthetic zero-world-flags test; all retail
TFX environments disable it with flag `0x0001`. A separate asset-free
lifecycle regression checks the final drawing window, delayed stage expiry,
owner notification order and validity of retained pool records.


All **187 regressions** pass in development, Release and ASan/UBSan after this
increment. No CI or CD playback has been added.


### Quit confirmation, external information and camera-relative tracking

Shift+Q in flight now opens the original quit/enrol dialog rather than exiting
immediately. No resumes flight directly, Yes exits and Enrol returns to pilot
selection. Physics and flight timing remain paused while this dialog is open.
The existing configuration dialog provides the authored widgets; shortcut
confirmation does not rewrite TFX.CFG. The isolated original dialog comparison
covers 200 complete pixel/state cases. A private-window check operates the
actual shortcut, verifies the paused simulation and exercises No and Yes.
Private-window tools now confirm the real dialog through shared input helpers.

D toggles the original external altitude/speed/heading strip. Its disabled
callback also leaves radio scrolling suspended, as in native 0x807b4. Shift+D
restores the mission's default stores and fifteen of each countermeasure;
it does not repair damage, add fuel or change the cannon navigation flag.
A controlled mission-502 restoration sequence agrees on captured state
and all pixels across 8 frames.

Air-lock projection now runs after the actual camera is selected, matching
0x92144 followed by 0x93328, instead of always using the forward-flight matrix.
Contacts are enumerated after refuelling has positioned the tanker too. An
8-frame F5/D sequence agrees on all captured state and every image pixel.
A 160-frame controlled mission-501 engagement also retains exact captured
state, smoke samples, procedural seeds, radio requests and every image pixel.
Player-model bindings are observed separately from their animation parameters.

The complete native capture formerly supplied an empty DID root. Original
0xc7424 adds its SPR subdirectory only when that root is nonempty, so that
host setup accidentally made all global texture pages unavailable. Captures
now supply an explicit virtual TFX root and normalize it at the hosted archive
boundary. The original loader, page allocation, binding and texture raster
execute normally; flight entry asserts all four retained global pages loaded.
The session comparator rejects caches made before this correction. The F5/D
and 160-frame engagement comparisons above use the corrected capture. Older
untextured frame comparisons do not establish original texture visibility.

The remaining keyboard audit identified a separate integration gap: native
Enter, Backspace/O and F/Ctrl change aerodynamic control configuration and
response gains as well as weapon/HUD state. Complete-session captures now
observe these configuration words and all four stored double bit patterns;
connecting those callers is the next control increment.


All 187 regressions pass in development, Release and ASan/UBSan. A private-window
training engagement completes normally after 507 real frames, including target
lock, AMRAAM release, enemy loss, objective credit and successful FINISH. The
older screenshot controls check expected Arcade to consume missile inventory;
that assertion is corrected to the original retained quantities, while its F8
camera step independently requires an actual launched store.


### Weapon-key flight responses and initialized movement limits

Enter now selects the original combat control configuration and response gains;
F/Ctrl selects cruise/navigation response. Backspace/O applies cruise response
when it selects a carried ground weapon, and leaves the existing response alone
when no carried weapon is available. F-117 selection retains the original second
selector call when the first result is its unavailable cannon. The native
selection sample is delivered for Enter and F/Ctrl. The cannon navigation flag
is independent of the last weapon role and survives mission transitions.

The response helper agrees with 24 original Enter/F executions: every aircraft,
all four flight models, the configuration word and all four stored response
double bit patterns. Its fixture contains inputs and expected numerical values,
without original game resources. Complete 24-frame keyboard/steering sequences
also agree on every captured field and pixel for the Eurofighter in Simple,
Military-spec and Realistic, and for the F-117 in Realistic. These cover Enter,
Backspace, F and Ctrl, the resulting trajectory changes and independent HUD flags.

The controls comparison also found that the IN RNG instrument reads the selected
pylon even during navigation mode. It must not substitute cannon range merely
because the gun/navigation flag is enabled. The live instrument now retains
that original selection rule.

Original mission entry 0x71138 overwrites the executable image's prelaunch
vertical movement limits with -16384..0 for TFX. Live camera contexts, sight
prediction and MFD orbit views now receive those initialized bounds. Using the
stored -8192..-32 image values made near-ground cannon predictions climb back
above ground after a movement clamp; the newly exercised F-117 empty-pylon sight
exposed seven incorrect pixels. The corrected 24-frame session matches all
original pixels. The isolated prediction comparison now includes that exact
one-subunit-below-ground boundary (513 trajectories, 2565 points). Another 2304
original MFD camera/overlay cases agree using initialized mission limits; the
original prelaunch-limit comparison remains separately available.


All 188 regressions pass in development, Release and ASan/UBSan. All 128 retail
missions also complete eight composed frames in Arcade and Military-spec without
scene warnings. These corpus runs check integration and memory safety rather
than original-session equivalence; the controlled comparisons above supply the
original state/pixel evidence.


### Paused tactical map and ILS map markers

M now opens original 0x86e2c's paused tactical map using the retained regional
schematic and DAT/COCKMAP.BIN overlay. It draws the player route, aircraft and
surface contacts, the first ten eligible enemy aircraft's complete telemetry
cards, and blinking target/ILS runway markers. The target blink colour persists
between openings; its timer resets. Original signed-word timer comparisons,
in-place redraws, route clipping and the initial runway marker's register-mix
quirk are retained. Shift+M replays the COMPLETE text (DATA 0x11b35), which is
distinct from the opening mission message.

`check_tactical_map.py` executes the original complete map painter and blink
loop. All 1,920 supplied frames agree on state and every indexed pixel across
the five regions. Cases cover contact altitude filtering, the ten-card limit,
timer wrapping, overlapping targets and runway markers. Contact enumeration,
timer/keyboard transport and the page flip are declared boundaries; original
sprite, overlay, line, font, heading and clear routines execute normally.

The live window pauses physics, temporarily presents the normal palette even
when flight uses infrared, and consumes its closing key. It preserves an
already-paused flight. `check_window_tactical_map.py` exercises those behaviours
through actual keys in a silent disposable installation and private display,
including held Escape/M/Enter and clean confirmed quit without rewriting CFG.
The development observer also compares the supplied RGB palette with the live
normal DAC bank after HUD brightness changes in infrared. Map entry now expands
that bank directly instead of reusing the last normal upload. Sanitizer DWARF
can omit the caller's bank; that observer reports the boundary explicitly and
still checks the presented colours, infrared return and modal behaviour.

Both the cockpit moving map and the new full-screen map now receive the ILS
landing cell rather than an unbound generic navigation marker. Surface contacts
follow original 0x7c7d0's side equality and exclusion of destroyed/retired actors;
neutral actors are omitted. The existing moving-map comparison still agrees
in 320 cases, and assembled HUD in 504 frames. A complete 12-frame F-117 ILS
on/off session agrees on every captured state and pixel.

All 188 regressions pass in development, Release and ASan/UBSan after the map
increment. The private-window map check also passes under the sanitizer build;
the ordinary configuration dialog still passes its live interaction check.
The retained 160-frame controlled engagement continues to match every captured
state and all indexed pixels.


### Original screenshots and remaining powerplant/cockpit shortcuts

The original 4 key now saves the displayed 320×200 image as a numbered
GRAB_00.LBM in the selected installation. Existing captures are preserved;
write failures are reported without terminating flight. The file retains
palette indices, eight bitplanes, ByteRun128 row compression, six-bit VGA DAC
colours shifted by two, and the original 5:6 pixel aspect. Infrared captures
use the displayed infrared bank. The developer PPM option remains separate.

`check_screenshot.py` runs complete original 0xdfe64 through hosted in-memory
DOS file operations and supplied displayed pixels. Twelve synthetic full files
match byte for byte, including header patching, compression fallback and all
palette entries. An independent IFF reader also checks their decoded pixels.
The asset-free capture regression covers headers/aspect, VGA masking, compressed
row boundaries and complete indexed-image round trips. The private-window check
presses 4 in paused normal and infrared flight, compares every saved pixel with
the displayed image and proves existing captures and CFG remain unchanged.

Slash now applies original 0x84de4/0x84e20's afterburner-off action, including
sound and SSD stop notification. Keypad 9/3 aliases Page Up/Down. The complete
key comparison exposed that keypad +/- also feeds the continuously sampled
throttle flags in 0x86458; the previously omitted keypad flags changed fuel
consumption and eventually speed after a reheat reduction. Those aliases are
restored. The engine-off path now delivers its afterburner-stop sample too.
Complete 24-frame Arcade and Realistic sequences agree on every captured field
and every indexed pixel, including reheat, fuel, trajectory, model parameters
and cockpit offsets.

Capture numbering uses ordinary decimal names beyond 99 and finds the first
unused name in the installation. The DOS implementation instead increments
its two filename bytes in place; its post-99 punctuation and process-local
filename cursor are not reproduced. This host-file convention has no effect
on gameplay or the encoded image.

All 189 regressions pass in development, Release and ASan/UBSan after this
increment. No CI or CD playback has been added.


### Original PLOP dispatcher and damage/loss gates

Holding right Shift and typing PLOP now toggles the original cheat. The matcher
consumes successful makes before P pause, L radio replay or O weapon selection.
No key and released right Shift retain partial progress; a mismatch while
right Shift is held resets it without retrying that letter as a new prefix.
Cheat state and partial progress belong to the application across missions,
matching the retained original globals. Activation/deactivation uses the
original radio strings.

The live gate reaches the independently checked original damage routine, the
player projectile-contact branch and the player-loss observer. Original
0x7d42b also resets the aircraft SSD every enabled frame before loss/ejection;
that call is retained. This is not blanket collision immunity: contact
explosions and already-started aftermath still follow the original rules.

`check_cheat.py` executes original 0x838dc in 4,096 persistent input steps,
including 202 toggles. Consumed makes, partial progress, the enabled flag and
all notice requests agree. The actual complete flight loop agrees on every
captured field and pixel over a 20-frame activation/deactivation sequence;
captures now observe cheat state explicitly. A private-window check types
right-Shift PLOP twice, proves matching P does not pause, and observes the live
player-loss gate turn on and off without writing game state through GDB.

Extending that sequence with an ordinary up-arrow exposed a separate existing
placeholder: the host invents relative mouse movement for Arcade arrows, while
the original Arcade callback consumes relative mouse motion. That input adapter
and the remaining modifier/keypad polling details are the next input audit.
The original cheat's Ctrl+D arming shortcut is not connected yet.

All 190 regressions pass in development, Release and ASan/UBSan after this
increment. The retained original damage and loss checks continue to cover the
enabled/disabled gates independently of the keyboard sequence.


### Persistent keyboard sampling and complete camera controls

Original 0x862f4/0x86458 now have a shared persistent keyboard poller. Right
Shift and Ctrl suspend manual direction/throttle refresh, retaining the prior
flags. Automatic landing also retains O; otherwise O refreshes independently.
Left Shift does not suspend manual input. The original camera pan word packs
right-Shift orbit/roll/distance controls into its low byte and Ctrl chase
translation into its high byte. Navigation-keypad aliases are connected too.
The controller preserves the target byte's high bits, as the native producer
does, for other consumers to own.

`check_keyboard.py` compares all 16,384 combinations of fourteen supplied held
inputs in a persistent shuffled sequence. Both original polling bodies execute;
only hardware key bytes are supplied. All manual flags, independent O and both
pan bytes agree. The asset-free regression covers retained input across
modifiers and automatic landing, independent release and high-bit ownership.

The live caller now supplies all these controls to the already compared camera
and flight kernels. Complete 32-frame Simple and Realistic sequences agree on
every captured field and indexed pixel: steering retained while panning,
left-Shift steering, keypad input, orbit distance, camera roll, Ctrl chase
translation and retained throttle. The previous Arcade arrow-to-mouse
placeholder is removed. Arcade always acquires relative mouse movement, as
its original callback does; other models retain their configured source.
The complete 24-frame cheat sequence, including its ordinary arrow input,
now also agrees on all captured state and every pixel.

The full input comparison still supplies one declared DOS make and a held-key
table per frame. It does not establish equivalence for multiple competing
makes/repeats within one host frame; GLFW edge sampling remains the host input
boundary. In-flight cheat rearming and recording remain separate work.

All 191 regressions pass in development, Release and ASan/UBSan. A private-window run
completes Hop 1 through normal pilot/arming/combat/debrief and the next-hop
campaign values in 507 real frames; the tactical-map input/palette check also
continues to pass.


### In-flight cheat rearming and current store metadata

Cheat-enabled Ctrl+D now follows original 0x85cb8 into the authored arming
screen, pausing physics and returning to the same flight. DEFAULT_WEAPONS
missions instead take 0x8b47c's immediate default-fit branch, without opening a
dialog or skipping a simulation frame. Both clear the original target/weapon
selection fields and reselect the appropriate initial role. Native 0x85d1d
still performs ordinary D/Shift+D afterwards: the information strip toggles,
or defaults and countermeasures are restored. Fuel and damage are retained.
The arming viewport handoff and consumed closing make-code are connected too.

The integration found a stale-metadata bug in the earlier flight binding:
replacing a weapon refreshed only its occupied flag, retaining its old mass,
drag and external-tank role. `armament::flight_stores` now derives the complete
mounted loadout from current inventory and the aircraft's physical mount
positions. Startup and each flight update use the same conversion. Visible
but depleted dispensers remain unoccupied; a nonzero quantity is a gate rather
than a mass multiplier. No flight arithmetic was changed.

`check_stores.py --inventory` now passes weapon IDs and quantities through that
binding before the existing x87 accumulator comparison. All 1,197 original
cases agree on stored output bits across all 27 weapons, three aircraft, ten
slots and mixed fractional mounts. The ordinary direct-accumulator mode remains
available. A separate asset-free regression changes a mounted missile to a
tank and checks refreshed mass/drag/role, quantities and empty/depleted slots.

The silent private-window rearming test uses actual right-Shift PLOP, Ctrl+D
and authored mouse controls. It derives a legal tank hit point from the original
SSD boxes and permission mask, places it, checks current inventory against live
flight-store metadata, then restores defaults. Physics remains stopped in the
dialog, and a held Escape returns without initiating ejection or writing CFG.
The original swept-pointer pick requires settling before the placement make;
the automation does so instead of clicking across intervening mount volumes.

A complete 24-frame Realistic mission-601 sequence agrees on every captured
state and pixel, including cheat activation, Ctrl+D's immediate default branch,
Shift+D, weapon selection and D's external-information side effect. This is
complete-session evidence for the immediate branch. The custom dialog relies
on the independent native arming widget/controller/picking checks plus actual
host-window integration; it is not a complete original modal-session comparison.

All 192 regressions pass in development, Release and ASan/UBSan. All 128 retail
missions render eight complete Military-spec frames without scene warnings.
The retained 160-frame controlled combat engagement still agrees on every
captured state and all pixels after the inventory binding change.

### Mission-to-mission launch state

The application now retains the live integer position, basic Mach value and
last WEAPONS index between flights. Original INIT supplies these only when
the next script omits a declaration; aircraft and angles still follow the
original reset rules. In particular, airborne mission 602 has no SPEED line.
The retained speed is the final live value, including camera/refuelling and
result callbacks, rather than the preceding script's starting speed. Inspection
does not change this flight state.

All 256 original initialisation cases still agree across fresh and seeded
contexts. The private-window training campaign additionally checks that Hop 2
receives Hop 1's actual final position and exact float Mach bits, plus its
WEAPONS index, after successful combat and debrief. A fresh eight-frame
Realistic mission-602 comparison includes the live speed bits and agrees on
every captured state and all pixels. All 192 development regressions pass.

### Complete-session targeting controls

Laser-designator entry and exit now use the original view queue instead of
switching the rendering path immediately. The old camera finishes its frame
before the queued view takes over. The direct ISO extra-key binding enters
without designating; Z designates only when the designator is already active.
Entry retains the original aircraft-state and carried-weapon gates. The gimbal
recentres after more than one absent input frame, and the designator suspends
ordinary held-key refresh while retaining previously sampled flight controls.

The designator paints after the ground-reference update and uses its updated
scroll phase. Its callback does not advance or draw the ordinary radio ticker.
X clears ground/mobile/radar selection and resets the radar cursor without
resetting the seeker reticle, projected point or lock timers. It also retains
the original lack of an aircraft-state guard.

`tools/check_targeting_session.py` retains three declared input sequences.
Complete 16-frame Arcade and 22-frame Realistic mission-601 captures match all
observed player, actor, random, control, weapon, seeker/radar, camera/horizon,
animation and projectile state, and every pixel in all 38 frames. They cover
entry, both zoom directions, designation, clearing, held steering, direct ISO
entry, cockpit return and re-entry. The independent mission-501 target-cycle
and clearing sequence supplies eight further complete frames. The existing
private-window test additionally exercises real device input, pause and
ejection from the designator without injecting any scenario state.

```sh
python3 tools/check_targeting_session.py --research ../tfx-reverse --build build
```

All 192 regressions pass in development, Release and ASan/UBSan. The private
designator window check passes in both development and the sanitizer build.

### Original recording codec

The original `.REC` encoder and bounded reader are implemented independently
of the live renderer. Forty-eight complete original files/96 frames agree
byte for byte; the original decompressor also restores all supplied fields.
The codec retains the 348-slot queue layout, 255-byte fill/literal runs,
2 KiB compression bound, resource-name dictionary and patched file offsets.
Malformed/truncated file checks run in the asset-free suite. Live recording
and the post-flight viewer are being connected separately. See
[recording.md](recording.md) for the layout and original-execution boundaries.

### Live flight recording and inherited graphics camera

Alt+V now streams `video.rec` using the original queue/file representation and
recording marker. Eight ordinary-view and eleven designator-view recorded
queues agree on every field after resolving runtime resource handles to model
names. Both complete live sessions agree on every captured gameplay state and
all 28 rendered frames. The additional 22-frame Realistic targeting sequence
also remains exact.

The original designator has a primary world pass using the inherited graphics
camera before its visible callback installs the new gimbal and paints another
world. Restoring that order is necessary for faithful recording, shader/jitter
state, actor animation, transient submission and seeker inputs. Both main and
MFD cameras retain their original setter angles and world anchor alongside the
derived basis; the scene retains the most recently installed camera. Ordinary
views continue through one primary pass. The full recording comparison and
the native trace boundaries are documented in [recording.md](recording.md).

All 193 regressions pass in development, Release and ASan/UBSan. Post-flight
playback remains the next integration step; the original path shows WNN and
press-conference animations before replaying the recorded model queues.
The 12-frame live recording comparison also passes under ASan/UBSan with
exact queues and pixels. Its read-only observer now captures the renderer
argument during the first direct-mission scene draw, avoiding reliance on an
optimised caller-local variable at the frame-return breakpoint.

### Post-flight WNN and recorded-flight playback

The original `wwn`/`prtv2` animations now precede automatic replay after a
recorded mission, before the campaign resumes. The bounded streaming reader
shares the checked original file codec; replay resolves the model dictionary
and paints the saved unsorted queues through the production renderer. It
retains the flight's drawing context and original infrared playback palette,
full-screen viewport, four-tick pace and blinking PLAY caption. Replaying a
flight does not advance gameplay. A new key press exits playback.

Eight ordinary-camera and eleven designator-camera original replay frames
match every pixel and the uploaded palette. The native reader also accepts
independently written files through the optional interoperability comparison.
A private-window campaign test recorded 508 frames using Alt+V, completed the
first training engagement through ordinary targeting/firing, observed both
WNN introductions and replay, and resumed the original debrief/next-hop path
with the expected inherited position, Mach, loadout and campaign values.
All 194 development regressions pass. Full recording layout, commands and
verification boundaries are in [recording.md](recording.md).

### Original recording filename editor

The configuration menu's filename field now takes focus, shows the native
underscore cursor and supports typing, backspace, scrolling and Enter. Its
18-character limit and both keyboard maps follow the original widget. The
unshifted branch lowercases letters; the shifted branch bypasses that call.
The filename belongs to the application, so menu and mission returns preserve
it without inventing a `TFX.CFG` key. Capture and playback use that shared name.
Presentation and dialog hosts now share their unchanged DOS make-code mapping.

The original widget comparison executes actual mouse/keyboard processing,
focus and text drawing: 115 supported edits and every complete dialog image
agree in development, Release and ASan/UBSan. All 200 original sound-enabled
configuration states/pixels remain unchanged. A private-window check edits
`QUICK.REC`, including Shift+Q while typing, records real flight frames to the
chosen filename and completes the existing preference-save, live model-change
and quit/resume checks. Nonprinting host keys do not embed NULs in path names;
the native comparison explicitly excludes PC-speaker tone/delay and cursor
hardware. All 194 regressions pass in development, Release and ASan/UBSan;
the private-window configuration check also passes under ASan/UBSan. Its GDB
host disables LeakSanitizer only for ptrace compatibility; ordinary sanitizer
regressions retain leak detection. See [recording.md](recording.md) for commands
and boundaries.

### Retail complete-session sweep

`tools/check_mission_sessions.py` selects the 128 retail scripts from the
checked mission catalogue and runs the complete original/C++ session
comparison, retaining each case, its log and a machine-readable result list.
`--capture-only` collects the bounded original traces independently of a C++
build; `--native-root` reuses those captures with the existing input/resource
hash checks. `--missions` selects a smaller batch. This is distinct from the
longer C++-only simulation/render smoke tests.

All 128 retail cases, sixteen Arcade frames each, now agree on all captured
gameplay/rendering state and every indexed pixel: 2,048 complete frames through
the actual launch and frame loops. Mission 850's original capture required a
larger instruction allowance; its successful retry preserves the same timer
inputs. Mission 507's comparison was rerun after isolating the sourced GDB
observer. This coverage does not establish later encounters or complete
campaign paths; longer controlled combat traces retain their separate gates.

```sh
python3 tools/check_mission_sessions.py --capture-only --output build/native-retail-sessions
python3 tools/check_mission_sessions.py --native-root build/native-retail-sessions --output build/retail-sessions
```

### Queued model lines through camera rotation and near clipping

The existing original authored-line regression is supplemented by 1,024
complete queued line-only programs, including 417 nonempty frames. Converter,
queue constructor, component/view matrix composition, scaled cache, model
program, projection, near-plane clipping, viewport clipping and VGA drawing
all execute in the original. Every pixel and all nine Q15 basis words agree.
Both apertures, behind/near/front endpoints, collapsed segments, all viewport
edges and rotated models are covered. Inputs deliberately stay inside the
original signed-word division domain; no faulted frames are used as evidence.

The existing production line painter already agrees for these inputs, so no
rendering change is needed. The additional 157 compact fixture rows retain
synthetic inputs and matrix/frame hashes, alongside the original authored-line
fixture. Older sprite-backed primitives and complete-session context retain
their separate verification gates.

All 195 regressions pass in development, Release and ASan/UBSan with the added
line-only fixture; this increment changes no production rendering behaviour.

```sh
python3 tools/check_model_transforms.py --lines
ctest --test-dir build --output-on-failure -R 'render.native_.*model_lines'
```

### Launch fuel and retained inventory

The wider retail-session sweep exposed a startup-order error in mission 604.
Original player/engine initialisation (`0x8c184`, `0x95f3c`) precedes arming
(`0x8b47c`): it reads the previous global pylon inventory. Fitting external
tanks increases capacity without giving an already airborne aircraft extra
fuel. Fresh mission 604 therefore starts with 4,000 fuel units, rather than the
8,000 units incorrectly supplied by initialising from its newly fitted tanks.

The host now retains the actual pylon inventory between flights and supplies
it to the next physical initialisation. Current mounted stores are installed
afterwards. Ordinary arming previews update store statistics; automatic
`DEFAULT_WEAP` fitting skips that preview update until the first flight tick.
Landed cold-start placement fills the current capacity later, after its
initial mass calculation. These distinctions matter even in Arcade mode.

`tools/capture_launch_stores.py` adds read-only entry observers to the ordinary
complete-session harness. Its `launch.json` retains fuel, inventory and raw
physical fields before/after the original arming calls. The C++ observer now
also checks initial fuel and, when these additional observations are present,
the pre-arming mass and capacity. No original game instructions or outcomes
are replaced by these observations.

All sixteen frames of mission 604 agree in all four flight models, including
every indexed pixel. The landed tank-equipped mission 762 also agrees through
sixteen Arcade frames, including its first-frame refill. A private-window
campaign test completes an actual AMRAAM engagement, records 507 flight frames,
passes both WNN introductions and replay, then reaches the next arming screen
with the final live inventory retained. That campaign test checks host
integration; it is not an original full-campaign equivalence claim. All 195
regressions pass in development, Release and ASan/UBSan.

Dense original mission launches can exceed the capture harness's former
instruction bound before sixteen frames complete. Its bounded execution
allowance is now larger; this changes no simulated timer or device input.

```sh
python3 tools/capture_launch_stores.py --mission 604 --flight-model realistic --output build/native-launch-604
python3 tools/check_flight_session.py --mission 604 --flight-model realistic --frames 16 --native-capture build/native-launch-604 --strict-pixels
python3 tools/check_training_engagement.py --campaign --recording
```

### Cannon command dispatch and longer combat observations

The queued Space command and joystick fire share original `0x83fa8`.
Its cannon branch fires before player physics; held Space is independently
polled later by `0x86898`. The first keyboard press therefore fires twice,
using alternating muzzles, while subsequent held frames fire once. The host
previously connected the early cannon branch only to joystick commands.
Both command sources now use it, retaining the separate weapon-system checks
and the later held-key path. Each actual shot also sends the missing normal
sample-13 request from original `0xa4ef8`.

`tools/check_combat_sessions.py` retains explicit keyboard tapes for an
intercept, ground designation/release and held cannon fire. The 32-frame cannon
sequence and 96-frame mixed cannon/Sidearm sequence agree with the original
in every captured state and indexed pixel, including ammunition, alternating
barrels, releases, projectile identities and trajectories. Release output also
passes the private silent PCM/window check with cannon firing, sound off/on
and shutdown. All 195 regressions pass in development, Release and ASan/UBSan.

The session observer now compares the real ten score/result counters, target
credit and cursor, violations, completion latch, exit countdown, timer and
termination flags. The 220-frame intercept reaches an actual missile kill
(frame 206), score/kill credit and objective completion (207), with all those
captured gameplay fields agreeing. The initial run exposed later drawing
differences; the ordering corrections below resolve those as well.

Projectile observation assigns identities at the real pool allocator's return
(`0xa2d79`), rather than at the shared constructor. HUD-only cannon predictions
use that constructor without allocating a live projectile and must not consume
observed gameplay identities. No prediction or game instruction is suppressed.

```sh
python3 tools/check_combat_sessions.py --case held-cannon --output build/cannon-session
python3 tools/check_combat_sessions.py --case ground-designation --output build/ground-session
python3 tools/check_window_audio.py --build build-rel --data-dir /path/to/TFX --cannon
```

### Shadow recording dispatch

The longer intercept's actual recording exposed an incorrect format value:
original constructor `0xcc238` stores drawing-path offset **56**, not 48.
Its dispatch entry `0xbacb1` installs the captured parameter bank and scale
before the ground-shadow renderer. Offset 48 enters a lower-level path that
skips those steps. The recorder and reader now use 56 for queued shadows;
original recordings containing them no longer fail as an unknown drawing path.

The original writer/reader comparison still agrees byte for byte across 48
complete files and 96 synthetic frames, now including the actual shadow value.
A real 60-frame original recording spanning the intercept and impact plays in
both implementations with every replay pixel and the infrared palette equal.
All 195 regressions pass in development, Release and ASan/UBSan. Live recording
queue comparison also identified the contact-tail correction below.

### Intercept contact tail and overlapping radar markers

Original `0xa384f` tests projectile-model life before actor contacts. Once that
gate passes, the terrain/effect tail still executes if an aircraft or surface
contact destroys the projectile. The host incorrectly tested life again after
the actor pass, omitting the resulting `clatter` effect. It now retains the
original pre-contact decision. This also preserves the authored effect's place
in the full 348-entry drawing queue; omitting it changes which distant scenery
survives the queue limit.

Horizontal radar `0x82fbe` traverses projectiles newest first. The host supplied
them in increasing identity order, giving overlapping enemy markers precedence
over the older player missile. The supplied span now follows native list order.
`tools/capture_radar_context.py` observes actual radar-entry inputs and phase
through the full original flight loop. The session comparator checks those
ordered inputs when `radar.json` is present, independently of sorted logical
identities used for general projectile-state comparisons.

The 220-frame controlled intercept now agrees in every captured state, all
ordered radar inputs and every indexed pixel in development and ASan/UBSan.
The 225-frame recorded variant additionally agrees on all sixty actual recorded
model queues and every live pixel, including the real missile hit, kill/score
credit and objective countdown. All 195 regressions pass in development,
Release and ASan/UBSan. These are complete flight-loop comparisons; full normal
mission return and broader campaign paths retain separate verification gates.

### Natural mission return and ejection frame ordering

`capture_native_flight.py --until-result` now follows ordinary control inputs
through the original loop's own termination flag, recovery, FINISH script and
teardown. `--frames` is an execution bound. The observer stops at `0x723d7` to
retain the final live snapshot, then resumes the same original call and stack
until it returns. No result, target life, score or campaign value is supplied.
The C++ comparison independently observes the actual FINISH variable table.

The successful mission-501 intercept agrees through **233 complete frames**:
missile kill at 206, objective completion at 207 and countdown return at 233.
Every captured gameplay field and indexed pixel agrees, including the final
score of 7,000, one credited air kill and successful mission result. Development
and ASan/UBSan both pass. Direct mission launches now initialise their actual
mission number and Arcade campaign flag, as the reference launch does.

The original countdown return also sets reported speed to zero and the landed
flag before its final drawing pass (`0x71bbb`/`0x71bca`); it does not move the
aircraft to the ground. That state now reaches the final C++ frame. The expanded
progress comparison checks those fields across **4,439 original updates in
512 sequences**, retaining compact original-derived hashes for ordinary CTest.

Double Escape followed by Escape after parachute deployment gives a second
natural return: **65 frames**, with recovered status 2, outcome 3 and `safe=1`.
All state, every pixel and the final campaign table agree in development and
ASan/UBSan. The closing Escape frame must still advance and draw before FINISH.

Ejection exposed two separate drawing-order mistakes. Player streams advance
in the frame tail (`0x70e44`), after their frozen camera submission and HUD
drawing. The abandoned aircraft uses a separate ordinary submission at
`0x71024`, before terrain; it must leave the general actor drawing list when
control switches to the pilot. Both orders are now retained. A recorded
68-frame ejection sequence agrees on every live pixel and all **66 recorded
model queues**, including seat separation and parachute deployment.

Camera observations now distinguish actual world-entry camera words from
horizon inputs. Ejection paints its horizon before installing the new camera,
so the old native projection cache at horizon entry is diagnostic state, not
the new world pose. The comparator checks both consumers at their actual
boundaries, using the projector's signed 32-bit Q16 representation.

```sh
python3 tools/check_combat_sessions.py --case training-result --output build/training-return
python3 tools/check_combat_sessions.py --case ejection-result --output build/ejection-return
python3 tools/check_combat_sessions.py --case ejection-recording --output build/ejection-recording
```

All 195 asset-free regressions pass in development, Release and ASan/UBSan.
Broader ground-attack, failure/recovery and campaign paths remain separate
whole-session verification gates.

### Retained sprite explosions and retail scope

The older sprite-backed model opcode 31 is present only in retained `explode`
and `explode2`. It belongs to the older explosion pool, whose initializer
`0x74f44` and producer `0x75030` return without loading/creating explosions
when environment bit 1 is set. All **33 decoded environments**, covering all
**128 retail missions**, set that bit. Retail TFX uses its SSD effect path.

`tools/check_legacy_effect_gates.py` executes each original environment parser,
then the actual old initializer, producer and painter with several supplied
heights. Neither active-list pointer changes, and no legacy model load or draw
is requested. An unexpected load/draw fails the check; it is not replaced with
a successful dummy consumer. This closes that path as a retail implementation
requirement, without claiming support for other retained engine configurations.

```sh
python3 tools/check_legacy_effect_gates.py
```

### Ground bombing, shared prediction targets and radar retirement

The controlled mission-502 sequence selects a GBU-12, designates ground through
the ordinary laser view, returns to the cockpit and releases at frame 70.
All **256 complete frames** agree in captured state and every indexed pixel,
including **226 actual projectile-motion target inputs** and ground impact at
frame 182. Mission completion remains covered separately by the intercept.

The first capture exposed an observer assumption: the original temporarily
reuses its air-guidance scratch pointer for the designated ground point.
Aircraft-identity observations now retain their aircraft-only meaning, while
the actual guidance coordinates are compared independently at the live motion
dispatch (`0xa32a6`). Temporary HUD predictions use the same movement helpers
but are excluded from that live-dispatch observation.

That stronger check found a real shared-state omission. Original HUD cannon
and bomb predictions call the ordinary projectile constructor. Although they
allocate no live projectile and consume no ammunition, they replace its shared
untargeted aim point. The runtime now owns those temporary prediction launches
and retains this side effect. The 32-frame cannon sequence agrees on all
**236 live motion inputs**, all captured state and every pixel.

The bomb-contact frame also exposed a four-pixel radar difference. The original
radar still visits impacted records before `0xa31a0` retires them in the frame
tail. The supplied projectile list now includes those records until that same
boundary, retaining the existing newest-first ordering.

### Original renderer exception delivery in the capture host

The original installs a divide-fault handler during renderer startup. Ground
bombing reaches three legitimate projection overflows, which the old capture
host could not deliver. `native_dpmi.py` now supplies DPMI vector storage and
the documented 32-bit exception frame. The installed game handler executes
unchanged, edits the saved return address and returns through the host.
`check_dpmi_exceptions.py` exercises divide-by-zero and quotient overflow at
all four recognised renderer fault sites: **eight original handler returns**,
preserving integer, segment, x87, SSE, control/debug registers, flags and stack.

Unicorn's interrupt hook bypasses its ordinary exception-delivery bookkeeping.
The host clears that internal pending state through a clean public context
restore while preserving the client's architectural registers. Otherwise a
second handled divide is incorrectly reported as a double fault. No original
instruction is patched and no rendering result is substituted.

```sh
python3 tools/check_dpmi_exceptions.py
python3 tools/check_combat_sessions.py --case held-cannon --output build/cannon-guidance
python3 tools/check_combat_sessions.py --case ground-bomb --output build/ground-bomb
```

### Model projection overflow

The original divide handler also establishes a rendering rule: a signed-word
projection quotient overflow returns from the entire current model, preserving
primitives already emitted. Later bytecode, material changes and jitter do not
execute. The C++ model interpreter now follows that early return, with the
actual camera aperture supplied explicitly; it does not fall back to a
continuous projection for these vertices.

There are **1,024 complete original-program comparisons**, split between flat
and Gouraud drawing, with large vertices, near depths and both apertures.
The original installs and executes its own handler. These checks compare every
pixel and drawing count, including programs that stop before or after drawing.
Two compact 140-case fixtures make the overflow paths available without game
assets. All 197 regressions pass in development, Release and ASan/UBSan; the
complete 256-frame bomb sequence also agrees with the guard enabled.

```sh
python3 tools/check_model_programs.py --overflow --cases 512
python3 tools/check_model_programs.py --overflow --gouraud --cases 512
```

### Flight-control handoffs and anti-radiation acquisition

A 64-frame live control sequence now agrees in all captured state and every
pixel in **all four flight models**. It selects waypoint, heading, tracking and
speed hold, changes altitude/speed targets, disengages and re-engages autopilot,
and changes stick-pressure/cruise settings. Target doubles are compared by
their stored bits, and MFD selections at their actual drawing boundary.
Realistic and Arcade sequences also pass under ASan/UBSan.

This exposed two caller omissions. Native `0x860ca` selects the left controls
MFD when autopilot engages. Native `0x7eb7c` displays AIRBRAKE from retained
deployment (`0x145a8`), independently of its switch status. Arcade can retain
that deployment after switching out of its temporary aerodynamic autopilot
callback. Both distinctions are now preserved. The ordinary B key also toggles
the actual deployment fraction, retaining fault statuses and aircraft without
airbrake drag. Sixteen complete brake-key frames agree in development and
ASan/UBSan. The original shared gear/airbrake switch sample is connected and
verified through a private silent Pulse/X11 device, including sound off/on.

A mission-503 anti-radiation launch agrees through **256 complete frames**,
including guidance, contact/retirement, all captured state and every pixel.
Native `0x932c4` automatically searches while the ground-target pointer is
empty. The caller now retries that acquisition instead of requiring a manual
C press before it can show LOCK. This trace does not destroy the distant SAM
site; ground-objective completion remains a separate coverage gate. A separate
64-frame sequence releases with no manual target-cycle press and agrees in
all captured state, guidance inputs and pixels.

```sh
python3 tools/check_flight_control_sessions.py --flight-model realistic
python3 tools/check_flight_control_sessions.py --flight-model military-spec
python3 tools/check_flight_control_sessions.py --flight-model simple
python3 tools/check_flight_control_sessions.py --flight-model arcade
python3 tools/check_flight_control_sessions.py --case air-brake
python3 tools/check_combat_sessions.py --case ground-radiation
python3 tools/check_window_audio.py --data-dir /path/to/TFX --air-brake
```

### Declared mouse and timer inputs

Complete-session tapes can now supply signed relative mouse deltas and the
original left/right button mask. The first frame primes desktop capture with
neutral motion. The reference consumes each relative sample once; the C++
observer supplies the corresponding accumulated GLFW position. Neither host
changes flight state or outcomes. A **32-frame mouse flight** agrees in every
captured state and pixel in development and ASan/UBSan, including steering,
throttle buttons, accelerated time and external/cockpit view changes.

The capture limit is now 1,024 frames. `--ticks-per-frame` declares a different
external timer interval; the original timing filters, clamps, acceleration and
projectile substeps still execute. Four ticks remain the default. A separate
sixteen-frame sequence with an eighteen-tick interval agrees in all captured
state and pixels. Bounded attempts retain their trace even if the requested
natural result has not yet occurred, rather than discarding useful evidence.

```sh
python3 tools/check_flight_control_sessions.py --case mouse
```

### Bound ground objectives and the final return frame

Mission targets bind once at launch, as original `0x70b70` requires: fixed
scenery in linked-list order, eligible mobile units in ascending slot order,
then terrain. Height is ignored for the exact cell-centre actor search.
Destroyed/retired mobile units are excluded. Actor searches truncate cells
to signed words, while terrain lookup retains the full signed dwords.
The live host now keeps these bindings instead of assuming every objective
is a terrain tile. **4,096 complete original binding executions agree**,
with 256 compact synthetic cases available without game assets.

A maintained mouse/keyboard tape flies mission 503 to its SAM target, releases
an AGM-88, destroys the target and returns naturally through FINISH. It enters
the original PLOP cheat through the actual key sequence to prevent enemy damage;
neither host supplies positions, targets, damage or outcomes. The external
timer interval is eighteen ticks, with the original time acceleration still
running. **All 338 frames agree in captured state and every indexed pixel**,
including 2,154 live projectile-guidance inputs and the final campaign table.

This also exposed an ordering error on the final frame. Original `0x720ef`
updates other aircraft before `0x71b90` expires the return countdown. Moving
that stage after AI preserves the player's flying speed for their last
update, before the forced landed/zero-speed return. All **4,439 original
mission-progress updates** still agree after separating the stages.

```sh
python3 tools/check_target_binding.py
python3 tools/check_progress.py
python3 tools/check_combat_sessions.py --case ground-strike-result
```

### Natural crash, descent warning and loss-camera displays

A second maintained tape uses only downward mouse steering and time
acceleration. The original hits terrain and returns through its aircraft-loss
FINISH in fourteen frames. **All captured state, HUD warning requests and
every indexed pixel agree**, including the resulting campaign status/outcome.

Three missing display handoffs were exposed. `0x7dde4` requests a vector
PULL UP HUD alert; `0x6f12` is its vertex table, not a radio string. The caller
now shows that alert and requests urgent sample 61. `0x7d349` also initializes
a fresh pitched orbit after queuing the loss camera; it must not inherit the
previous camera basis. Finally, viewports with no display callback neither
paint nor advance the retained radio ticker. The crash trace now observes
alert delivery separately from its later painting.

The impact frame's MFD also reaches zero altitude. Watcom prints rounded
decimal zero without a sign, whereas host printf produced `-0`. MFD telemetry
now uses the same verified decimal conversion as the other HUD displays.
**732 complete original MFD pixel/coverage frames agree**, including signed
zero, small positive/negative values, both positions and all three aircraft.

```sh
python3 tools/check_combat_sessions.py --case crash-result
python3 tools/check_mfd.py
```

### Connected Gravis effects and original interrupt-driven PCM

The `SOUNDFX=4` path now uses an emulated one-MiB GF1, distinct from the
single-stream Sound Blaster path. All 103 named resources fit in resident RAM;
voices 1–3 rotate for resident requests while voice 0 remains reserved for the
original streaming arrangement. Lower-RAM streaming is not selected or exposed.
The original active-voice count is 32, whose oscillator frequency base is 19,293.
All four logical voices receive the native volume 256. The initial resident
cursor is one, so the first request selects voice two.

The translated register driver loads rate and volume tables from the user's
`GUS.COM`; no original instructions execute in production. The pinned DOSBox
GF1 core is isolated from its DOS frontend. Its chip clock, stereo pan and
unsigned-to-signed DMA transfer remain separate from the host's 48 kHz output.
A bounded lock protects requests and device callbacks. Disabling effects rejects
new requests while allowing an already accepted queue to finish.

`tools/native_gus_device.py` boots the actual relocated GUS.COM in a private
16-bit DOS environment, stops before child launch, and delivers real DMA/wave
IRQs through its installed handlers. All four RAM capacities detect correctly.
The allocator leaves destinations unaligned, but command 0 writes only
`address >> 4` to the GF1: later uploads can overwrite the preceding sample's
last fifteen bytes. The translation preserves that behaviour. Every byte of
synthetic aligned/unaligned transfers and the stopped stereo tail agree across
256/512/768/1024 KiB.

`tools/native_gus_samples.py` additionally executes the game's C52D0/C4F5C/DDE2C
loader and DDF68 callback installation. Original TFX allocates, copies and patches
its 144-byte real-mode client; that client runs unchanged when the original
GUS.COM interrupt handler ends a voice. The reference supplies only DOS/DPMI
transport and declared PCM file contents. Queue arithmetic, scheduling,
callbacks, allocation and driver instructions are not replaced.

8,192 complete original register operations agree, including semantic reads,
reverse addresses, dword frequency wrapping and test-authored volume tables.
The complete queued comparison agrees for 207 actions and 214,170 stereo
frames, including overlapping requests, rejected priorities, signed cooldowns,
disabled submissions, varied callback sizes and a 30-item sequence. All 106
retail slots and 103 actual resources additionally agree over **526 actions and
551,160 frames at 48 kHz**, in every PCM sample and observed queue field.
These comparisons validate the translation against original code on the same
GF1 core; they are not physical-card recordings or independent proof of that
core's analogue output.

```sh
python3 tools/check_gus_device.py --build build
python3 tools/check_gus_driver.py --build build
python3 tools/check_gus_samples.py --build build
python3 tools/check_gus_samples.py --build build --retail --rate 48000
python3 tools/check_window_audio.py --build build --data-dir /path/to/TFX --gravis --cannon --air-brake
```

The register and test-authored PCM/state fixtures run without game assets as
`audio.native_gus_registers` and `audio.native_gus_pcm`. All **202 CTests** pass
in development, Release and ASan/UBSan. The silent private Pulse/X11 test verifies
live gear, airbrake and cannon output, GF1 stereo pan, sound off/on, preserved
configuration and clean shutdown. Optional QSound and broader complete-flight
audio coverage remain separate work; CD playback remains deferred.

### Optional QSound dependencies

`Q2.COM` SHA-256
`24d626ce3a436296cd4cbfeb4123c8eb0b26765cb73f6678346512fb064edbbf`
is a flat real-mode program, distinct from the Gravis helper. Its RIFF/CSP
strings are parser tokens, not an embedded QSound firmware image.

Original CS:0962 main calls CS:070D, which first searches the directory named
by the DOS `SOUND` environment variable. CS:05A8 tries
`\CSP\WFM0001z.CSP` down through `WFM0001a.CSP`, examining RIFF function/code
chunks for a supported format. CS:039A separately opens the character device
`CSPXXXX0`, requests its far entry point through DOS IOCTL 4402, and invokes
that driver to allocate/load/start processing. The helper publishes magic
`62951413` and the driver pointers only after successful setup.

`tools/check_qsound_dependencies.py` executes unchanged main against three
initialized C-runtime environment roots, supplying DOS file-not-found at the
external file boundary. All 26 names are searched in each case and no interface
is published. A separate unchanged CS:039A run establishes the absent-device
failure. Neither path substitutes a hardware-detection or processing result.

```sh
python3 tools/check_qsound_dependencies.py --output build/qsound-dependencies.json
```

The retained TFX installation has neither those Creative firmware files nor a
CSP DOS driver. The [Linux ALSA CSP implementation](https://github.com/torvalds/linux/blob/master/sound/isa/sb/sb16_csp.c)
likewise loads microcode into a separate hardware processor and sends QSound
codec parameters. The three per-sample TFX parameter words do not provide that
algorithm. The reconstruction currently plays unprocessed SB samples; QSound
processing is explicitly unavailable. Accurate support would require the
appropriate external firmware and processor implementation or independently
verified processing behaviour. This does not prevent ordinary SB16 playback.

### Longer interception sessions in the other flight models

The maintained `training-simple-result` case runs the same declared T/Enter/
missile controls through the full Simple model. The original downs its target
and returns naturally at frame **238**, with score **7,000**, one air kill and
a successful FINISH outcome. Every captured state, guidance input, cockpit
input, indexed pixel and final campaign value agrees in development and
ASan/UBSan.

The same launch misses in the original Military Spec and Realistic models.
The maintained `training-military` and `training-realistic` cases retain their
**384-frame** trajectories, including enemy attacks, rather than forcing a
successful result. All captured state and every indexed pixel agree in both
development and sanitizer comparisons. These
remain direct-mission contexts with the declared inherited Arcade campaign
flag, not proof of career-mode court-martial or campaign completion.

A one-frame airbrake-condition mismatch in the initial Military Spec check
was an observer error: it read the manual flight update's cached input after
a later missile hit had changed the authoritative system condition. The
observer now reads the same system byte represented by the original. New
captures additionally retain all twelve system conditions, repair clocks and
accumulated damage bits for complete-frame comparisons. No gameplay logic was
changed to resolve this observation.

### LAPC-I flight effects and playback ownership

`SOUNDFX=2` selects the original synthesised effects arrangement. Original
launch wrapper 72970 calls A5FC8 to load filename-table entry 17, normally
`tunes/fxonly.rld` with `MUSIC=2`. It suppresses sampled Sound Blaster output
and ordinary flight music. Effect wrapper A6068 starts the requested order
at volume 75. Its main-song handle is initially invalid, so successive
effects can overlap and exhaust the shared 32-channel pool; a full pool
silently rejects another request, as in the original.

The live consumers now include engine start/stop, afterburner increase, gear
and airbrake switches, cannon, missile/bomb releases and the engine-powered
F6 fly-past. Slash stops afterburner without requesting a synth effect.
The sampled-sound switch gates recorded effects/speech; the music switch
suspends and resumes the synth effects clock.

Original resource release leaves active songs playing. The sequencer now
retains immutable resource ownership in each playback slot, independently
of public handle reuse. A synthetic bank verifies twelve complete master
snapshots across active release, continued ticking, handle reuse, overlap,
all sixteen playback slots, capacity failure and ordered stopping. This
does not establish the original's behaviour when a different bank overwrites
its shared DOS buffer before an old effect has finished.

```sh
python3 tools/check_lapc_effects.py --build build
python3 tools/check_osl_lifetime.py --build build
python3 tools/check_gear_key.py --build build
python3 tools/check_flight_session.py --build build --mission 501 --frames 32 \
  --inputs tests/inputs/lapc-effects.json --lapc-effects \
  --rom-dir /path/to/roms --strict-pixels
```

The component comparison executes the unchanged original 32-bit wrappers,
16-bit OSL master and LA callbacks. All **512 MIDI events and sample
timestamps** agree through overlap, exhaustion, music mute/resume and bank
reuse, with 546 shared IRQ ticks. The private-window comparison separately
checks every state, indexed pixel, radio request, damage field and LAPC cue
in 32 complete flight frames; host PCM is muted before its device starts.
This is original-code agreement, not a physical-card recording.

That session also exposed the Eurofighter's overspeed gear warning and
automatic gear switch sound. The shared gear command now preserves original
warning precedence, the 350-speed limit, aircraft exceptions and flight-state
gates. All 4,096 original gear-command calls agree, with a 256-row retained
fixture. The lifetime and gear fixtures bring the asset-free suite to
**204 passing tests** in development, Release and ASan/UBSan.

### Career-rule complete flights

Window ownership no longer selects permissions or replenished stores. The
inherited presentation `arcade` variable now supplies those rules throughout
flight, independently of the numerical flight model. Ordinary direct launches
still declare Arcade rules; developer `--mission=N --career-rules` declares
`arcade=0`. It supplies an initial context, not an outcome or persistent pilot.

The maintained `career-crash-result` and `career-intercept-result` comparisons
execute the same context in the original. All **14 loss frames** and **233
successful interception frames** agree in state, radio, damage, indexed pixels
and recovery/FINISH campaign values, in development and ASan/UBSan. The latter
consumes its missile, receives permission at frame 138 and returns with score
7,000 and one air kill.

The interception exposed an aircraft-index error in the permission notice:
native simulation aircraft 1 means the F-117, whereas mission-script aircraft
start at 1 for the Eurofighter. The game and analytical input conversion now
retain that distinction. All 8,192 original permission-handler cases agree;
the compact retained fixture includes the corrected aircraft exceptions.

```sh
python3 tools/check_combat_sessions.py --case career-crash-result --build build
python3 tools/check_combat_sessions.py --case career-intercept-result --build build
python3 tools/check_engagement.py --build build
```

These are complete direct flights with declared career rules. They do not
establish every campaign graph or court-martial presentation branch.

### Complete engine-key sound requests

Original engine handlers 842B0/84370 now have a shared typed controller. They
honour the current system condition and refuelling/loss gates. Switching off
the last normally stopped partner also stops reheat. Engine stop requests
urgent sample 59; startup requests the ignition/on sequence 77,60 at normal
priority. The two original handlers retain their different ordering of that
sequence and the LAPC startup cue.

All **4,096 complete original calls** agree on power factors, system and
afterburner state, display refresh, sample priority/sequence, LAPC cues and model
notifications. A 256-row fixture adds `flight.native_engine_keys` to CTest.
The last remaining literal gear-speed threshold now uses its shared constant.

Flight also restores original 83970's background-priority sample-8 click before
bound key actions. Consumed PLOP letters bypass that dispatch as in the original.
The complete-flight harness now observes actual original variadic sample
requests and the C++ PCM voice entry, including requests rejected by the
selected null PCM device. It separately checks ordered sample IDs/priorities
and LAPC cues. Each live synth check uses a private silent Pulse sink and mutes
its PCM device before start.

```sh
python3 tools/check_engine_key.py --build build
python3 tools/check_flight_session.py --build build-san --mission 501 --frames 32 \
  --inputs tests/inputs/engine-controls.json --lapc-effects \
  --rom-dir /path/to/roms --strict-pixels
```

The 32-frame engine sequence agrees in Arcade and Realistic flight under
ASan/UBSan, as does the separate 32-frame gear/brake/weapon/fly-past sequence:
all state, radio, damage, pixels, recorded-effect requests and LAPC requests.
Private Release PCM checks also pass for Sound Blaster and Gravis: live gear,
airbrake and cannon, sound off/on, preserved configuration and clean shutdown.
All **205 asset-free regressions** pass in the three build configurations.

### Longer career and landing investigations

The maintained career airspace sortie uses actual PLOP, time-warp and reheat
keys. Original execution reaches its natural orders-violation FINISH at frame
740. C++ now agrees on **every captured gameplay state, warning, final campaign
value and all 740 indexed images**. The former discrepancy began at frame 511,
where a single moving-map border column retained different cockpit pixels as
the schematic left the viewport. The map painter itself already agreed in
**640 full pixel/coverage frames**, including map edges and all VGA alignments;
the missing behavior was the display-page lifetime described below.

### Ground service and complete landing captures

The original `71E44` landing-service controller is connected after completion
checks and before the other actors. A stopped, landed aircraft is serviced
once, provided it has not crashed, ejected, resolved its mission or violated
orders. Initial ground starts are already marked serviced; taking off clears
that latch. Fuel, ammunition, countermeasures and system conditions are
restored, the score receives the original 10,000-point credit, and missions
without DEFAULT_WEAPONS reopen their real arming screen. Its closing input
and elapsed UI time are consumed before flight resumes.

All **8,192 complete original eligibility calls** agree, including persistence
through repeated stops, takeoff and initializer changes. Refit, output delivery
and physical initialization are declared consumer boundaries in that isolated
check. Its 256-row regression is the 206th asset-free test.

    python3 tools/check_ground_service.py --build build

Complete approaches separately exercise the real initialization and frame
loops. Mission 511 reaches its runway finalizer after 237 frames; mission 512
uses the same Columbia runway and takes 246, despite its carrier instruction.
Mission 1111 provides a genuine 130-frame carrier landing, including actual
DEFAULT/DONE input in its post-landing arming screen. The capture stops only
after original touchdown finalizer `A02C8` executes and the complete frame
returns. An unexpected mission loss is retained for diagnosis, rather than
being presented as a successful approach.

These comparisons exposed missing caller behavior: Alt+L must invoke the F5
handler's complete cockpit-departure sequence, and the direct ILS destination
selector must emit its engaged announcement. Both the key/toggle path and the
direct selector now agree in **1,536 original calls each**.

    python3 tools/check_landing.py --build build
    python3 tools/check_landing.py --build build --destination-only

Ground service retains the original route/guidance scratch. It also reuses
the mission's authored initial-ground flag: an airborne-start mission can
therefore reset to its cell centre at fixed speed 250 after servicing. This
surprising original behavior is retained. The renderer basis exported before
service survives for that frame even though the physical basis is reset.
The cockpit fuel gauge likewise survives initialization; an immediate
automatic approach must not replace its reading with an uninitialized zero.

The complete **237-frame Simple runway approach agrees in every captured
state and indexed pixel**, including the service frame. The **130-frame
Realistic carrier approach agrees in gameplay state, radio, damage, stores,
camera and horizon**, and its final image matches after the real arming dialog.
The later submission, thin-face and world-input corrections below resolve its
five earlier image differences. The **246-frame Raptor runway case** likewise
has complete image agreement after the thin-face correction, including its
previously differing frame 14.

    python3 tools/check_combat_sessions.py --case career-airfield-landing
    python3 tools/check_combat_sessions.py --case career-carrier-landing

The carrier command keeps strict pixel checking enabled. The absence-of-runway case in
mission 605 separately reaches its original refusal and natural aircraft-loss
result after 36 frames; its captured gameplay/FINISH values agree, with small
palette-index differences on six intermediate images subsequently resolved by
the model-submission ordering correction below.

### Retained display pages and the shared cockpit refresh clock

Original VGA output rotates through three retained pages. The native page
segments in the first four frames are A3E8, A7D0, A000, A3E8, with 16,000 bytes
per plane/page. Repainting a single host framebuffer and unconditionally
recopying the cockpit footer erased pixels that DOS kept between page visits.
The live renderer now preserves three complete indexed buffers, rotates them
after completed drawings, and leaves paused frames on their current page.

The footer and instrument caches also share native byte `10BED`. Reheat keys,
view changes and MFD page selection can request another three-page refresh.
Native `74104` compares the raw cockpit Y with its retained value, requests
refresh when it changes, then decrements the byte before painting overlays,
the optional footer and instruments. The host now passes that same clock
through those owners. Overlays still draw when the footer is retained.

All **8,192 original refresh-decision prefixes** agree, including repeated Y,
signed boundary values, byte countdowns and resets. The capture stops before
the original clamp/painter suffix; it replaces no instructions. A 512-row
asset-free fixture is the 207th regression. The independent static-frame check
also agrees on **396 complete pixel/coverage/offset/origin images**, covering
both copied and retained footers. These component checks complement the full
740-frame career comparison above; they do not substitute for it.

    python3 tools/check_display_pages.py --build build
    python3 tools/check_cockpit_frames.py --build build --retained
    python3 tools/check_combat_sessions.py --case career-airspace-result

The subsequent world-input corrections below resolve the carrier trace's
remaining intermediate world polygon differences.

### Original model-submission order through complete flight frames

The original scene controller `7572C` submits ascending slots from the other-air
pool, then allied air, surface actors and the fixed-scenery list. Creation order
can interleave those pools. Scene handles retain their allocation identities,
while an explicit drawing sequence now preserves that original submission order.
This matters even for unrelated landscape polygons: equal-depth quicksort ties
depend on the complete input sequence.

Main-loop calls `72135` and `7213A` also submit the current player model and the
abandoned aircraft before `A26CC` queues the sky. That order applies independently
of which camera/coordinate constructor produced the player model. An offscreen
sky model can still change the queue's equal-depth ordering.

The complete **36-frame mission 605 loss/refusal sequence now agrees in every
captured state, indexed pixel and natural FINISH value**, in development and
sanitizer builds. All 2,851 queue insertions agree in priority, payload slot and
order. A separate **96-frame missile-camera sequence** uses actual launch/F8
inputs, confirms entry to native viewport 6, and agrees in every captured state,
queue insertion and indexed pixel. All 207 asset-free regressions also pass in
development, Release and sanitizer builds.

    python3 tools/check_combat_sessions.py --case career-no-runway-result --model-queues
    python3 tools/check_combat_sessions.py --case training-missile-view --model-queues

`--model-queues` observes original `C4930` and the production C++ queue before
their allocators/sorters run. It excludes arming-screen previews, which have
separate UI comparisons. Queue ordering and final images are both checked;
neither is substituted for the other. New session captures also retain the
original view mode. The subsequent corrections below resolve the carrier's
remaining discrepancies.

### Thin Gouraud faces and the planar empty-span rule

One carrier-approach discrepancy isolated a complete shaded F-22 triangle with
identical projected coordinates, palette attributes and lookup row in both
renderers. The original painted nineteen pixels; C++ painted one. A narrow
scanline near its bottom had crossed integer endpoints, and C++ incorrectly
stopped the entire polygon before drawing its wider rows above.

The painter selected by original `BD95C` first rounds the right endpoint to its
four-pixel VGA plane group for that early-return test. An empty span inside the
same group skips its row; a crossed group ends the polygon. The integer Gouraud
path now preserves that distinction without changing its palette interpolation,
plane-edge rounding or the separate flat/texture paths.

All **1,024 original thin-face frames** agree, covering each plane phase, small
faces, varying attributes, reversed winding and viewport clipping. The existing
**1,024 full-word attribute frames** and **1,024 varied-depth projected Gouraud
frames** also agree. A compact thin-face fixture brings the asset-free suite to
**208 passing regressions** in development, Release and ASan/UBSan.

    python3 tools/check_gouraud_polygons.py --thin
    python3 tools/check_gouraud_polygons.py --wide-colours
    python3 tools/check_world_polygons.py --gouraud --warped

### Terrain anchors and independent ground-height writes

Main-loop `72149` supplies terrain traversal with integer camera anchors
`134F0`/`13500`, independently of the smoothed Q16 projection translation.
Using the latter shifted the entire selected grid by one row on two carrier
approach frames. The scene now retains that distinction; new complete-session
captures compare the original traversal anchors directly with the C++ inputs.

SSD opcode 73 writes global ground height independently of whether an object
reports a surface contact. Its result is now an optional height: a written zero
and no write have different meanings. The caller consumes each write in order,
separately from contact presence. Original main-loop `7210A`/`72111` also clears
presence and height before each new collection. Retaining the preceding height
after leaving a carrier was incorrect, even when no visible shadow exposed it.

The SSD comparison now observes actual original writes to `ABF2`, in addition to
the resulting value. All **19,589 persistent states across 703 definitions**
agree, including write presence. Regression cases retain height-only writes,
written zero and non-player/missing queries. All **4,800 original world-contact
queries** also agree across terrain rotations and map seams.

The retained training script **1111** provides a complete Realistic carrier
approach and real post-landing DEFAULT/DONE arming interaction. All **130 frames,
9,575 queue insertions, terrain anchors, shadow ground heights, captured gameplay
state and indexed pixels agree**, in development and ASan/UBSan. This closes the
five earlier carrier image discrepancies. It supplements the retail mission
coverage; this retained script is not one of the 128 retail-associated missions.

    python3 tools/check_instances.py --build build
    python3 tools/check_world_collision.py --build build
    python3 tools/check_combat_sessions.py --case career-carrier-landing --model-queues

The original visibility, geometry, sorting, collision and landing routines run
unchanged in their reference captures. Correcting supplied inputs and preserving
independent outputs replaces no original result with an expected outcome.

### Complete camera, weather and displayed-palette comparisons

A fresh sweep after the world-ordering, thin-face and terrain corrections agrees
across all **128 retail missions and 2,048 complete opening frames**, in captured
state and every indexed pixel. Longer C++ stability sweeps also cover every retail
mission with up to 8,192 simulation steps in Realistic and Military-spec modes,
the latter under ASan/UBSan. Those stability runs are not original comparisons;
missions which naturally finish stop advancing before that upper bound.

Controlled Realistic camera sequences now agree through **160 view-changing
frames** and a separate **96-frame target-camera flight**. The observer now labels
native `135C8` as acquisition and `135D0` as confirmed guidance, matching the
independently checked sensor owner. An acquiring candidate can exist without a
confirmed lock; the observer previously printed those two fields in reverse.

    python3 tools/check_flight_control_sessions.py --case views
    python3 tools/check_flight_control_sessions.py --case target-views

Complete captures now observe the actual VGA DAC writes at ports `3C8`/`3C9`,
alongside every indexed image. The C++ observer reads the palette supplied to the
production framebuffer's RGB conversion. Independent six-to-eight-bit expansion
then checks all 768 colour components per frame. `--strict-pixels` rejects either
image-index or displayed-palette differences. Legacy captures without DAC data
retain their narrower indexed-image comparison.

An **80-frame infrared/designator/external-view sequence** exposed a retained
anchor distinction even while its final images agreed. The designator's `B9A6C`
call changes the projection cache, while the ordinary world pass still uses the
last normal view's `134F0`/`134F8`/`13500` terrain and lighting anchor. The C++
ordinary pass now preserves that anchor independently of the moving gimbal.

    python3 tools/check_flight_control_sessions.py --case palettes

The complete-session tools accept the seven original `REALISM` values explicitly
and observe the live settings after initialization. Native `95F3C` unconditionally
clears wind at `96411`, including after ground servicing. C++ now retains that
startup rule; wind can subsequently be enabled through the original flight menu.
A saved enabled flag alone does not establish an active-wind flight.

Mission 507 with precipitation, temperature and recovery enabled agrees through
**64 complete Realistic frames** in development and ASan/UBSan: all captured state,
rain requests, raw wind initialization, indexed pixels and displayed palettes.
This verifies the storm and startup reset, rather than claiming wind was active.

    python3 tools/check_flight_session.py --mission 507 --flight-model realistic \
      --realism 0,1,1,1,1,1,1 --x87-backend build --frames 64 --strict-pixels

`--x87-backend` declares hardware adapters for the original FPATAN, FSIN, FCOS and
FPTAN instructions. Unicorn narrows those instructions to binary64; the existing
adapter preserves their actual 80-bit inputs/results for raw floating-state
comparisons. Original surrounding instructions, initialization, wind controller,
flight calculations and drawing still execute. This is a reference dependency,
not a production physics replacement or a relaxed comparison tolerance.

All **208 asset-free regressions** pass in development, Release and ASan/UBSan.

### Live weather changes and defensive flight controls

The complete-session input tape now supports declared logical mouse clicks inside
the configuration screen opened by backtick. Original `94C8C`, both dialogs,
widget hit testing, option callbacks and `A6808` preference writing all execute.
Only external mouse returns, relative motion and capture-owned file output are
supplied. The C++ comparison operates its real configuration screen through GLFW
input returns. Neither side receives an assigned setting or flight outcome.

A **96-frame Realistic storm flight** switches wind on, off and on again through
three actual menu visits. Every captured state, raw wind value, rain request,
indexed pixel and displayed palette agrees in development and ASan/UBSan. The
final configuration also agrees byte for byte. This supplements the saved-flag
startup/reset comparison above with genuinely active wind.

    python3 tools/check_flight_weather_sessions.py --case live-wind
    python3 tools/check_flight_weather_sessions.py --case live-wind --build build-san

The defensive-control sequence spends both fifteen-cartridge inventories, attempts
further releases with empty stocks, and presses E/S repeatedly. Complete **64-frame
comparisons for each playable aircraft** agree in captured state, live decoy
requests/origins, projectile lifetimes, shared randomness, HUD inputs where
captured, indexed pixels and displayed palettes. The F-22 comparison also passes
under ASan/UBSan. Two caller/context errors were exposed:

- Native `75784` publishes the shared player contact through `75114` immediately
  before actor updates. Earlier key handlers consume the previous published
  record, including its zeroed cold-start position. Using the newly initialized
  aircraft position for an immediate first-frame decoy was incorrect. This
  contact now has application ownership, separate from physical flight state,
  and is published at the original point after ground servicing.
- Native ECM/stealth handlers compare the zero-based aircraft index with one:
  the **F-117** ignores E/S, while Eurofighter and F-22 accept them. The isolated
  key checker had incorrectly supplied mission-script values directly. Its
  index conversion and 256-row regression fixture are corrected; all **8,192
  original key-handler cases** agree with the proper aircraft identities.

    python3 tools/check_combat_sessions.py --case defensive-controls
    python3 tools/check_combat_sessions.py --case defensive-controls-eurofighter
    python3 tools/check_combat_sessions.py --case defensive-controls-raptor
    python3 tools/check_defence.py --build build

New complete captures observe the published contact independently of the physical
pose, and the defensive HUD's actual inventory/mode inputs. The live X11 test now
uses an F-22 to exercise its permitted mode changes; real key input, HUD counts,
ECM motion inputs, stealth sensor suppression and clean shutdown pass under
ASan/UBSan. The successful **233-frame career interception** and **130-frame
Realistic carrier landing** still agree in every captured state and indexed
pixel after the ownership correction, the latter under ASan/UBSan.

All **208 asset-free regressions** pass in development, Release and ASan/UBSan.

### Complete refuelling encounter

The complete-session observer now records the original refuelling controller's
enable/saved-mode bytes, phase, reconnect delay, follow divisors, selected tanker,
proximity result and attitude errors. These are compared independently of the
already observed aircraft poses, fuel bits, model states and cockpit inputs.

A **256-frame complete flight** agrees in development and ASan/UBSan in every
captured state, indexed pixel and displayed palette. Ordinary throttle inputs
establish separation, apostrophe engages the controller, connection occurs on
frame 69, fuel transfer starts in the following physics update, and the frame-241
toggle disengages it. Fuel subsequently falls through ordinary consumption.

    python3 tools/check_refuelling_session.py
    python3 tools/check_refuelling_session.py --build build-san

This is explicitly a **test-authored mission**, `tests/scenarios/refuelling.mis`,
not a claimed retail training completion. It loads Columbia and the unmodified
`miss500` tanker routes, with a nearby F-117 start. Both executables load the same
source through their normal resource loaders; no controller state, pose, fuel or
outcome is patched after initialization. The test uses an unused mission number
and refuses to shadow an archived mission. Captures identify the test source by
SHA-256. The fixture retains CRLF because the original tokenizer recognizes CR
as its end-of-token marker; LF-only source makes that parser run past its lines.

The approach also retains the original launch snapping to map-cell centres and
the controller's wrapped dword squared-distance calculation. Docking anchors use
the native signed high-word products: Q15 rotation components shifted by sixteen
halve those model-coordinate offsets. Treating them as ordinary Q16 coordinates
would place the approach incorrectly. These are retained reference behaviours,
not production changes made to help the test connect.

### Saved language selection at startup

The CLI now uses the original `DATA:AB1C` language-directory ordering:
English, French, German, Italian and Spanish for `LANG=1..5`. Italian and Spanish
had been reversed in the startup selection table, even though the independently
selected caption/planner renderers already agreed in all five languages. One
shared table now handles validation and saved-number selection.

    python3 tools/check_startup_languages.py
    python3 tools/check_startup_languages.py --build build-san

The check reads expected directory names directly from the original executable,
then observes the actual front-end entry after normal configuration/CLI parsing.
It reproduced `LANG=4` incorrectly choosing Spanish before the fix. All five saved
selections and five explicit overrides now pass in development and ASan/UBSan.
The executable's help also reflects the already corrected ECM/stealth aircraft
restrictions and Arcade's mouse steering.

### Hostile damage, engine indicators and the loss camera

Mission 602 now has a maintained **120-frame complete hostile encounter**. Only
the ordinary time-warp key is pressed. Incoming weapons appear from frame 18;
the unattended player suffers hits, loses systems and power, falls, hits scenery
and returns through FINISH on frame 120. Development and ASan/UBSan match every
captured state, indexed pixel and displayed palette, including the complete result
table.

    python3 tools/check_combat_sessions.py --case hostile-loss-result

This interacting sequence exposed three context distinctions:

- Basic-flight throttle/fuel read the live engine factors, while their target
  speed penalty at `8CADA` reads the instrument status bytes. A freshly failed
  engine loses power before its status is normalized at the frame tail. Those
  inputs now remain separate through Arcade, Simple and disabled-aircraft
  callbacks. **432 original factor/status combinations** agree in exact speed
  and throttle bits and form an asset-free regression.
- Damage accumulation must enter Watcom's `127F` floating environment. Its
  isolated checker had omitted original FPU initialization and consequently
  compared both sides using the host/default extended arithmetic. Startup is
  now included, the production routine uses the existing scoped environment,
  and the **8,192 damage/repair references** were regenerated from that original
  execution. Small accumulated values and 40/80/97-percent reports explicitly
  exercise the rounding distinction.
- The loss orbit follows the published `DATA:12B88` record. `7D3F8` refreshes it
  on the ordinary loss path; the earlier contact-explosion return skips that
  write. Using the current physical position moved the impact frame's camera
  too soon. The host now retains this independent target through that transition.

    python3 tools/check_basic_engine_indicators.py
    python3 tools/check_damage.py
    python3 tools/check_player_flight.py --early-engine-faults

The early-fault sequence agrees through **4,608 complete original outer player
callbacks**, across 48 initialized scenes, all three aircraft and all four flight
models. It injects system conditions at the damage boundary before autopilot
handover; it is distinct from the naturally hit mission above. All **209
asset-free checks** pass in development, Release and ASan/UBSan.

### Post-flight campaign control flow

    python3 tools/check_campaign_segments.py

**680 campaign segments across 96 real mission subroutine call sites** agree in
action order, numeric variables and return stacks in development, Release and
ASan/UBSan. Expected execution uses the original branch, assignment and return
handlers with the retail graph and its actual suspended caller. Cases cover
normal/retry paths, death, missing-in-action, safe ejection, repeated-ejection
discipline, court-martial, promotion and defeat.

Flight outcome/status variables are explicit supplied inputs in this comparison.
Media are observed rather than painted, and execution stops at the next external
launch, menu, planner or child-parser boundary. This broadens progression
coverage without claiming those 680 cases are naturally completed sorties or
whole front-end playthroughs. The complete flight/result, serialization and
private-window save/continue tests remain separate evidence.

The Release private-window front-end check also passes after these fixes: name
entry and Control qualification, DOS pilot saving/reloading, the main menu, FWS
briefing/custom arming/flight, and Tour squadron selection/route review/flight.
The Runtime install component stages only `tfx`; its Release binary starts and
omits developer options. CI remains unconfigured.


### Pilot slot menu font

The load/save slot panel selected presentation style 5, a title font, for its
heading, pilot rows, statistics and overwrite/delete prompts. This produced
oversized glyphs and overlapping numbers despite the static bitmap labels being
correct. The panel now uses style 6, matching the original small-font measurement
and drawing calls at `D2AFC`/`D2B54`.

    python3 tools/check_pilot_panel.py

The new comparison exercises all ten selected rows on both load and save pages,
plus occupied-slot overwrite and inactive-pilot deletion prompts: **27 complete
indexed frames agree pixel for pixel**. Test-authored DOS saves and matching
original slot summaries supply the records. Archive background loading, pointer
setup and physical VGA page changes are declared host boundaries; the original
heading, row, alignment, shadow, statistics and prompt painters execute unchanged.
The reference installation and its pilot files are never written.

The separate font check still agrees on all **1,104 glyphs and 132 strings**.
Pilot creation/loading/replacement, persistence and progression checks pass, as
does the private-window enrolment/save/reload/briefing/arming/flight sequence.


## Arming preview transforms, mouse axes and square pixels

The arming renderer now uses the same native Q15 model basis, Q16 camera
translation, detail selection, facing tests and distance-palette rows as flight.
Its previous floating transform halved model vertices independently of camera
translation. The main aircraft and miniature pointer consequently appeared too
small. The weapon thumbnail view also had pitch/yaw exchanged; the miniature
pointer's fixed camera roll had been assigned to its actor instead. F-117/F-22
component anchors now retain their SSD world-axis placement while their bodies
rotate, using the existing scene placement and orientation helpers.

Original `89210` adds relative mouse Y to pitch and X to yaw. `898EC`/`89927`
map absolute mouse X to aircraft X and mouse Y to Z, retaining the original
height and query origin. Those axes now agree for rotation, store placement
and clearing. The host uses GLFW's disabled cursor during rotation, retains
unbounded logical coordinates for continued movement, preserves slow movement
at large window scales, and releases capture on cancellation, exit or focus
loss.

    python3 tools/check_arming_previews.py --output build/arming-preview-check
    python3 tools/check_arming_controls.py
    python3 tools/check_arming_picks.py
    build/arming_preview_probe /path/to/TFX flow
    python3 tools/check_window_pixels.py
    python3 tools/check_frontend_window.py --data-dir /path/to/TFX
    python3 tools/check_window_rearming.py

The complete preview comparison runs the original model/SSD loaders, camera
setters, animation, shape interpreter, rasterizer, palette lookup and widgets:
**99 complete arming frames agree in every indexed pixel**, covering all three
aircraft, one full miniature-pointer revolution, store-thumbnail rotation,
weapon-bay animation and screen-edge clipping. DOS files, allocation, external
devices and VGA page transport remain declared host boundaries. The same
comparison passes in RelWithDebInfo and Release. Controller comparisons cover
**1,039 original mouse/placement conversions**, **3,072 control sequences** and
**7,241 widget hit tests**; SSD selection still matches **12,860 swept/boundary
queries** and **60 bay-animation states**. Actual pointer selection, mounting,
cancellation and exit preserve the chosen paired stores on all three aircraft.
The private-window rearming check also observes updated flight-store mass,
drag and role after mounting a tank.

The pre-flight TFX logo is `iff/raw/tfxstat.lbm`, loaded by original `7D594`
after arming; it is independent of the startup intro. Its resource and forty
palette fade stages are confirmed against that original routine. Normal
front-end launches now show it even with `--skip-intro`.

Window presentation uses square 320×200 pixels, integer nearest-neighbour
scaling and centred margins when resized. Private X11 checks verify uniform
pixel blocks at scales 1, 2 and 6, automatic desktop fitting, a non-multiple
997×743 window and real menu clicks through its margins. The frontend test
also checks rotation capture, focus-loss release/reacquisition, custom pylon
placement and the loading-logo transition before flight. It uses disposable
installations and never sends input to the user's desktop.


### AGM-109H automatic mission targeting

The cruise-missile branch of native `93328` calls `93128` before aircraft-radar
and stealth checks. The flight loop now executes that separate controller:
it locks the mission's first bound target automatically, using its initialized
SSD aim offset and a height ten world units above ground. Cycling the ground
preview with N does not redirect the cruise destination. Its 32,767-unit
horizontal projection cutoff hides the reticle without discarding the lock.
The same designation supplies the HUD, range indicator, F9 observer and missile
runtime; it is kept separate from aircraft-pool identities.

The forward HUD's lock-acquired result now reaches the audio owner. Native
`7FC50` requests urgent sample 74 (`tracking`) for the cruise destination and
non-leader contacts; aircraft formation leaders use normal-priority samples
74 and 71 (`lookslik`). Cheat rearming also preserves its restored flight
palette, avoiding an extra fixed-palette blend and its accumulated rounding.

`check_cruise_target.py` executes the original acquisition and point projector
in 4,096 cases, including exact range boundaries, signed wrapping, invisible
points and retained timers. A compact native fixture adds
`sensors.native_cruise_target` to CTest. The forward-HUD comparison now includes
weapon 19 alongside the other ground stores.

`check_cruise_session.py` uses actual Shift+PLOP/Ctrl+D input and arming-screen
mouse actions to mount AGM-109H on an F-117 in mission 703. All 24 complete frames
agree in captured gameplay state, indexed pixels and displayed palettes,
including the tracking request, F9 ground-target view and the original refusal
to fire while landed. The input transport observes the end of the opening fade
before clicking, and resumes its external clock after the modal return; no
inventory, target or outcome is injected. `--airborne` supplies an explicitly
authored airborne start to exercise actual release and guidance separately;
its additional 24 complete frames also agree in every captured state, pixel,
palette and sound request.

```sh
python3 tools/check_cruise_target.py --build build
python3 tools/check_cruise_session.py --build build --rom-dir roms
python3 tools/check_cruise_session.py --build build --airborne --rom-dir roms
```


### Incomplete-mission landing notice

Landing for service before completing the objective enters the original arming
screen with `MISSION INCOMPLETE RE-ARM PLANE`. Native service wrapper `8B5E0`
installs callback `8A66C`; startup and cheat rearming use the ordinary idle
callback instead. The first left click dismisses the notice without activating
the widget underneath. Right click also dismisses it; Escape retains the
original empty-loadout fallback.
The wording comes from the executable, and its caption uses the original
panel dimensions and text placement.

`check_arming_previews.py --incomplete` executes the service wrapper's entry
and compares 99 complete original/C++ frames across all three aircraft,
including dismissal over DONE without click-through. `--dismiss right` checks
the outer loop’s earlier right-click cancellation. The full flight observer
also records which entry callback actual landings and cheat rearming select.
The carrier-landing case reaches the notice at frame 130; all 130 complete
flight frames agree in gameplay state, indexed pixels and displayed palettes.

```sh
python3 tools/check_arming_previews.py --incomplete
python3 tools/check_combat_sessions.py --case career-carrier-landing
```


### Offline game-timed music exports

The `music_render` target pairs the production OSL sequencer and `music_clock`
with the same FM, Munt and Sound Canvas devices used by the game. It settles
reset before loading the resource, begins at order zero and volume 75, and
renders through a boundary measured independently by the original shared IRQ.
Backwards jumps exclude the first repeated notes; a real driver stop supplies
the following two-second release tail.

The research batch independently executes original MEDIT/device code for every
resource. All 104 hardware auditions of the 81 resources agree in the complete
16-field playback-state trace and exact rationally scheduled sample boundary.
The 23 SCC resources each have separate SCC-1A v1.30 and SC-55 v1.21 exports.
Every 48 kHz/24-bit, maximum-compression FLAC was decoded and compared to its
floating-point render after the recorded fixed gain. Identical audio shares a
file, leaving 100 stored FLACs; native captures and source/ROM/renderer provenance
remain separate. The audio-browser check covers the four intro players, hardware
filters, actual SC-55 FLAC loading and the distinct SCC-1A file.

```sh
cmake --build build-rel --target music_render -j
python3 ../tfx-reverse/tools/render_music_archive.py
python3 ../tfx-reverse/tools/map_music_scenes.py
```


### Fetched reference CD and streamed soundtrack

The asset helpers download the Nova Rev 1 reference dump from Internet Archive
as fifteen individual archive members, with pinned SHA-256 values in
`scripts/game-files.txt`. They never download or unpack the enclosing 7z.
The bundled CUE retains the source INDEX 00/01 positions under short local
filenames. The existing hash checks, repair, retries and User-Agent apply to
these images as well as the ROMs. No disc or ROM bytes are committed.

On a normal first launch, `resources::cd_image` reads the Mode 1 ISO filesystem
from the fetched CUE/BIN set. `install_reference_cd` extracts only the eight
runtime assets, using the reference DISK1.DAT member offsets; it does not run
the DOS installer. Later launches compare and repair those assets from the
managed disc without replacing TFX.CFG or pilot saves. A fresh installation
selects the original SB/CD mode. An existing installation's choice is retained.
The default source is `cd/tfx.cue` beside the selected game data, then in the
default data directory. Explicit `--cd-cue` and `--cd-audio-dir` also select
CD mode unless `--music` overrides it.

CUE tracks are bounded by their own INDEX 01 and the next INDEX 00, or the end
of their FILE. Both shared and split BIN layouts work. CD-DA's 44.1 kHz stereo
PCM streams through the same decoder/resampler as local numbered FLAC, WAV,
Ogg Vorbis and MP3 rips. Track filenames use physical disc numbers, starting
at 02. A worker decodes into a bounded ring; the shared 48 kHz output callback
only mixes available PCM. No whole-track conversion, temporary audio file,
codec initialization or file read occurs in that callback.

The original zero-based MSCDEX index is converted to the physical track at
the transport boundary. Flight entry uses pending music index minus eight,
or native index eight (physical track 09) when none is pending. Native A5374
polls stopped playback strictly after 300 shared timer ticks and restarts the
selected track; music enable restarts it too. Synthesised presentation music
and SB/CD flight music retain their distinct roles. Authored presentation
CD play/stop requests also reach the same player. This supersedes the earlier
notes above that deferred CD delivery.

`check_cd_control.py` executes the original launch music block, CD polling,
cue gating and enable/disable handlers. All 274 control steps agree with the
production runtime, including the signed timer arithmetic, restart delay,
mute/unmute and flight return. MSCDEX and PCM transport remain explicit
boundaries; the C++ probe retains the real OSL driver with its FM register
writes ending at a transport sink, without constructing unrelated FM hardware. The independent audio check compares raw image PCM exactly,
compares lossless rips exactly and compares Vorbis/MP3 against FFmpeg within
codec rounding differences. All sixteen source ISO files match the retained
hashes; all eight installed runtime assets match the original installation
byte for byte. Repair checks preserve configuration and pilot files. The
asset-free CTest covers CUE pregaps, streaming, EOF and replay. The image/rip
checks and all 274 control steps also pass with address/undefined-behaviour
sanitizers enabled, including leak checks.

The normal default-path launch was checked with a disposable HOME/XDG data
directory, including initial extraction and subsequent repair. Private X11
and PulseAudio checks exercise CUE playback and Release FLAC playback through
the real flight loop and shared output, without accessing the user's desktop
or audible output device.

```sh
python3 tools/check_fetch_assets.py
python3 tools/check_cd_audio.py --reference ../tfx-reverse
python3 tools/check_cd_control.py
python3 tools/check_window_music.py --data-dir /path/to/TFX \
  --music soundblaster-cd --mission 501 --cd-cue /path/to/tfx.cue
```


### Defined DBOPL initialization arithmetic

The pinned DOSBox core calculated channel/operator byte offsets through null
pointers. The isolated adapter now uses `offsetof` and element sizes, with
standard-layout assertions. Its attack-rate search also intentionally wraps
signed fixed-point products; the target now explicitly requests `-fwrapv`
instead of depending on undefined overflowing multiplication. Neither change
alters the original synthesis algorithm.

`check_fm_pcm.py` preserves SHA-256 PCM baselines captured before either
change, for all 27 retail FM resources: 256 shared IRQ ticks and a two-second
release tail each, at 48 kHz and native game volume 75. Every PCM byte and
sequencer trace agrees after both changes. The new asset-free
`audio.fm_initialization_and_pcm` regression constructs real DBOPL hardware
and compares native/resampled dual-mono output across callback partitions.
The complete FM player lifecycle also passes strict ASan/UBSan and leak checks,
including 2,832,000 PCM samples compared across callback sizes.

```sh
python3 tools/check_fm_pcm.py
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 build-san/fm_synth_test
```


### Continuous CD playback through live configuration

The configuration loop reapplied the current music switch on every redraw.
Unlike the synthesised player's idempotent enable operation, the original CD
music-on command deliberately restarts the selected track. An open menu was
therefore repeatedly restarting CD playback. The host now applies changed
sound/music switch values once; explicit music-on still uses the original
restart controller.

`check_audio_menu.py` reproduced the failure before the fix and passes after
it. Its test-authored stereo ramp identifies actual CD position while the
sum channel isolates ordinary mono SB effects. The private live window checks
continued music through an open menu, off/on restart and continued advance,
simultaneous airbrake/cannon playback, independent effect muting/resumption,
a single shared PCM device and clean shutdown. The recording monitor uses a
bounded latency so assertions inspect current PCM. No user's profile,
desktop input or audible output is used.

```sh
python3 tools/check_audio_menu.py
```


### Persistent campaign and long-session closeout

`check_campaign_journeys.py` executes original branch/assignment/return and
save/read handlers continuously across 25 complete controller journeys. The
production parser and actual pilot store agree on 175 explicitly supplied
flight returns, 1,095 native-byte save/reload checkpoints and 3,225 observable
state/stack/save records. Cases cover all campaign groups, training graduation,
retries, safe/repeated ejection, discipline, death/MIA, promotion, defeat,
Arcade lives/log restoration and wrapped score arithmetic. This is controller
coverage: flight results and menu selections are declared inputs, while media
and planner actions are observed boundaries.

`check_campaign_flights.py` separately restores a disposable pilot at an authored
pre-flight checkpoint in Flashpoint mission 901 and Tour missions 801/821/841.
It respects DEFAULT_WEAPONS rather than imposing a fitting screen. Real key
input reaches ejection/parachute and natural recovery/FINISH. Original parser
handlers then predict the retry launch from the observed result, including its
actual numeric campaign state and the exact authored pilot-save bytes. GDB
only observes; no damage, result, flight state or timer is injected. The existing
naturally completed training/recording test also passes through the successful
kill, original WNN/replay and next-hop position/Mach/inventory inheritance.

All three synth modes agree with original controller/master/device execution
through 128 persistent flight/menu lifecycles each: 61,440,000 audio frames
(21 min 20 s), 69,914 shared IRQ ticks, retaining phase across all resource changes. Exact event/sample
counts are FM 540,027, LAPC-I 181,547 and Sound Canvas 246,137. Resource/filename
loads are 1,536, 384 and 1,024 respectively. These are device events and timing,
not a claim of comparison to analogue hardware recordings. Repeated LAPC effects
agree separately on 48,895 exact MIDI events/timestamps through 128 lifecycles,
with original setup/cue/release and OSL/LA callbacks.

`check_projectile_runtime.py --frame-tail --recycle --frames 4096` compares
32,768 complete population frames through a deliberately constrained 17-slot
pool. Every nine frames it attempts another batch, exercising full-pool
rejection, retirement/reuse, child dispensers, designation and jettison. The
original constructor, motion, list order, retirement, shared RNG and retained
HUD warning references execute. Anchors and deterministic swept-contact losses
remain explicit boundaries. Development and strict ASan/UBSan results agree.

The [verification matrix](verification.md) consolidates the current claims,
reproduction commands and intentional omissions without retaining the old
chronological backlog as current work.

### Connect G-strain breathing to flight audio

The live palette painter already implemented the original blackout/redout
colour changes and returned the native breathing request, but flight discarded
that request. Both ports now submit it through urgent sample playback, matching
`0x73a0c` → `0xa4dfc`. Sample IDs 65–67 (`breath1q`–`breath3q`) retain their
authored cooldowns and the existing scheduler's priority/preemption rules.
Paused redraws do not submit new requests. The original blackout selector's
second threshold still reads the redout word; no threshold or palette changes
were made. G effects remain a separate saved realism setting.

The native G-controller and palette comparisons pass unchanged. The private
window/audio regression supplies G-load only at the controller input and
checks its real visual and audio consumers, including recovery and paused
view changes. See [verification](verification.md#g-effects-and-breathing).
