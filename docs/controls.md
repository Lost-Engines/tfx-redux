# Controls and configuration

These follow the original September 1994 TFX 2.05 controls. Start the game with `--data-dir /path/to/TFX`; use `--help` for command-line options.

In the arming screen, click the aircraft to rotate it with the mouse; click again or right-click to stop. Rotation captures the mouse inside the window and releases it on focus loss. Select a weapon, then click an available pylon to mount it; right-click ends placement. Escape cancels the active operation first. From idle preflight arming it returns to the preceding choice screen; during in-flight rearming it asks before returning without applying changes.

The saved `LANG` values are 1 English, 2 French, 3 German, 4 Italian and 5 Spanish. `--language=NAME` overrides the saved choice for this run.

Startup reads the original `TFX.CFG`; `--flight-model` overrides its model selection. Without a configuration, Arcade is selected. Arcade uses mouse steering. Select `--flight-model=simple`, `--flight-model=military-spec` or `--flight-model=realistic` for the other original models, using arrow keys and Alt+left/right for rudder. These retain their different flight dynamics, aircraft-specific handling, gear damage and carrier arrestor behaviour. `--control=keyboard`, `--control=mouse` or `--control=joystick` overrides the configured input in the non-Arcade models. Simple supports keyboard/joystick steering; joystick steering is available in the three non-Arcade models. `--joystick=N` selects a particular device (0–15), otherwise the first available stick is used. An absent stick falls back to keyboard input.

Joystick buttons follow the original mapping: fire, select ground weapons, select air weapons and cycle targets on buttons 1–4. Stick sensitivity uses the original `JOYSCALE` setting.

Press **Escape** or **backtick** in flight for the original configuration and realism menus. Changes take effect when you return to the game, and preferences are saved to the installation’s `TFX.CFG`. The dialogs include flight model, controls, joystick sensitivity, world detail, recovery and realism settings. The sound switch controls the original sampled effects and speech.

`A` toggles autopilot. `7` selects waypoint following, `8` heading/altitude hold, `9` aircraft tracking and `0` speed hold. Arrow keys adjust applicable autopilot targets; `=`/`-` adjust target speed. `6` restores cruise speed, `5` toggles automatic stick pressure and Tab requests recovery. Alt+L requests automatic landing at the selected runway or carrier. Cockpit control displays show the live autopilot, flaps and F-22 thrust-vectoring state.

Hold either mouse button or `=`/`-` to change thrust. `[` and `]` toggle the engines, `G` operates the gear within its original speed and flight-state limits, `H` toggles the arrestor hook, `W` toggles wheel brakes and `B` operates the air brakes. Keypad `*` increases afterburner; Alt+keypad `*` or keypad `-` decreases it. Enter selects air-to-air weapons, Backspace/O selects air-to-ground weapons, and F/Ctrl selects the cannon/navigation mode. C cycles radar contacts or anti-radiation targets; Space fires the selected weapon. Alt+J jettisons air-to-ground stores and external fuel tanks. Insert releases chaff and Delete releases flares. On the Eurofighter and F-22, E toggles ECM and S toggles stealth; enabling one turns the other off. The F-117 retains stealth and ignores those two mode keys.

Apostrophe toggles aerial refuelling: the nearest allied tanker supplies the original contact cues, connection behaviour and fuel transfer. I/backslash toggles the original instrument landing system, with approach-beacon and runway/glide-slope guidance. Z opens the laser designator, arrows aim it, comma/period change zoom, and another Z designates the ground point. The original T/L indicators show search and designation status. X unlocks and F1 returns to the flight view. Direct `--mission` launches use the mission's default stores; normal campaign launches use your chosen loadout. Weapons retain the original pylon order, with a moving lock-on reticle for guided air-to-air missiles. The original forward HUD shows flight readings, heading/pitch scales, NAV/AA/AG weapon sights, cannon trajectory and bomb-impact prediction, plus angle of attack and climb rate in the advanced flight models. Selected aircraft show their original telemetry and acquisition/lock cues; designated ground targets use the original outline, and mission ground targets show their projected cross and direction cue. `Page Up`/`Page Down` raise and lower the original cockpit. `1`/`2`/`3` cycle the original MFDs, `R` changes radar range, and `N` selects the ground-target preview. The displays show live stores, fuel/range, systems, moving maps and radar contacts; FLIR/DLIR and ground-target previews use the current software scene renderer. Radio calls scroll through their original cockpit display; `L` replays the last message. `T` toggles the original fivefold accelerated time; steering temporarily suspends it. Airspace warnings and penalties apply near the map boundary. `P` pauses and releases the mouse. Press Ctrl+Escape twice to eject, or choose EJECT in the Escape menu and confirm; the seat animation, parachute descent and recovery region determine the pilot’s result. Ctrl+Escape after ejection ends the flight. Escape opens the flight menu without abandoning the pilot. Shift+Q opens the original quit/enrol confirmation; No returns directly to flight. See `--help` for the remaining controls and options.

`F1` restores the cockpit; Shift+F1 gives a full forward view. `F2`/`F3` turn your head, `F4` looks behind, `F5` follows the aircraft, `F6` places a fly-past camera, `F7` selects tactical view, `F8` follows the latest fired store (press twice for its nose view), `F9` observes a target and `F10` enters the virtual cockpit/target-tracking view. In external views, moving the mouse orbits the aircraft, fired store or target; it controls the camera instead of steering the aircraft. The mouse is captured until you pause, open a menu or switch away from the game. Selecting a camera again resets the mouse orbit. Cockpit, virtual-cockpit, designator and missile-nose views keep their existing controls. Right Shift+arrows turns the view; Home/End changes distance and Page Up/Down rolls supported views while right Shift is held. Ctrl+arrows/Page Up/Page Down moves the chase camera. Keypad navigation keys also work. The virtual cockpit uses the original canopy and static instrument textures. External views show the original altitude, speed and heading strip; D toggles it. Shift+D restores the mission’s default stores and countermeasures. Aircraft also show their carried weapons from the current loadout. Shift+N toggles the original infrared night vision; the laser designator and fired-store nose view also select their original infrared palette. Altitude-based colours follow the original environment palettes; startup uses the saved `HUDCOL` intensity and Y/U adjust it.

`M` opens the original tactical map, pausing flight and showing the route, targets, aircraft and surface contacts. Press any key to return. Shift+M replays the mission’s completion message. The map and cockpit moving maps show the selected landing runway while ILS is enabled.

`4` saves a numbered `GRAB_00.LBM` screenshot in the game directory, preserving the active palette. Existing captures are kept. Slash switches afterburner off; keypad plus/minus also changes thrust, and keypad 9/3 raises/lowers the cockpit.

The original cheat is available: hold **right Shift** and type **PLOP** to toggle it. Its original damage protection and collision behaviour are retained. With the cheat enabled, Ctrl+D opens in-flight arming; missions that require default stores refit those immediately.

The 3D render size follows the window by default; `--resolution=WIDTHxHEIGHT` fixes it explicitly. Set the main camera’s horizontal field of view with `--fov=DEGREES`, and visibility with `--draw-distance=CELLS` (`0` or `infinite` for the whole map). Cockpit artwork and text use nearest-neighbour scaling with square pixels; MFD scenes render at their displayed pixel size. The floating window fits the available desktop; `--scale=2` starts a 640×400 window, or doubles an explicit render size. Mouse, keyboard and joystick input are released on focus loss, while simulation and playback continue. Press P or open the menu to pause.

Alt+V starts or stops a lossless video recording of the displayed view, without changing those rendering settings. `--replay FILE` opens a saved recording directly; add `--skip-intro` to omit its WNN prelude. See [recording details](recording.md).


## Sound

Sound Blaster FM music follows the original `MUSIC` setting, and `--music=soundblaster-fm` selects it explicitly. For Roland LAPC-I music, use `--music=roland-lapc` with compatible CM-32L/MT-32 control and PCM ROMs. `--music=none` disables music. The original Sound Canvas setting uses SCC-1A emulation; `--music=roland-scc1a` or `--music=roland-sc55` selects the available hardware model, with its matching ROMs found automatically beside the game files or in the default installation directory. `--rom-dir` overrides that lookup for custom locations. These are compatible alternatives to the original SCC-1, whose exact ROM set is unavailable. Flight music now uses the original theme selection, temporary cues and engine sounds, including engine playback with music switched off. The original SB/CD mode uses synthesised presentation music and CD flight music; the explicit FM option selects synthesised flight music.

For Gravis sampled effects and speech, set `SOUNDFX=4` in the installation's `TFX.CFG` and retain its original `GUS.COM`. This emulates a one-MiB card, with the complete sound bank resident. Music selection remains separate.

For the original LAPC-I effects arrangement, set `SOUNDFX=2` and `MUSIC=2`, with the Roland ROMs in either of those automatic locations. It uses the original synthesised effects bank during flight, replacing the usual flight music and recorded speech. Leave `SOUNDFX=1` for Sound Blaster effects and speech alongside Roland music.

### QSound spatial effects

For Sound Blaster effects (`SOUNDFX=1` or `3`), `--qsound=auto` enables the built-in QSound model; this is the default. `--qsound=off` selects the ordinary dual-mono DAC path. `--qsound=on` explicitly requires a supported Sound Blaster selection.

The recovered numerical coefficients are compiled into the application. QSound needs no firmware download, file lookup or `--rom-dir` setting. The fetch scripts still supply the ROMs needed by the separate Roland synth emulators.

This reconstruction uses the original 8 kHz coefficient bank at each sample's DAC rate, and the recovered timer-driven pan controller. Position 33 and larger position bytes clamp to the rightmost recovered position, 32; the controller retains its unsigned wrapping. Clamping and fixed-point arithmetic are explicit compatibility choices, with [verification limits](verification.md). The effect processes recorded speech and sampled Sound Blaster effects only; Gravis, MIDI synthesis and CD music bypass it. It is intended for stereo speakers and also emits cancellation audio on the opposite channel from a virtual source.

### CD soundtrack sources

The fetch scripts supply `cd/tfx.cue` and its individual BIN tracks in the default
game directory. The game streams their audio directly and prepares the reference
runtime files without an external extractor. Existing configuration and pilots
are retained. `--music=soundblaster-cd` (alias `cd`) selects the original SB/CD
mode; the original introduction still uses synthesised music.

`--cd-cue FILE` selects a different CUE/BIN image. `--cd-audio-dir DIRECTORY`
selects local numbered FLAC, WAV, Ogg Vorbis or MP3 tracks. Either source option
selects CD mode unless `--music` explicitly overrides it. Choose one source,
rather than supplying both. Rips retain physical CD numbering: the first audio
track is `track02`, with an appropriate extension. `track-02`, `track_02` and
case variations also work. If several formats exist for a track, FLAC takes
precedence, followed by WAV, Ogg and MP3.

## Developer commands

RelWithDebInfo retains the developer scene inspector and headless captures:

```sh
./build/tfx --data-dir /path/to/TFX --presentation cdintro
./build/tfx --data-dir /path/to/TFX --menu mastopts
./build/tfx --data-dir /path/to/TFX --inspect
./build/tfx --data-dir /path/to/TFX --cell=136,114
./build/tfx --data-dir /path/to/TFX --flight-frames=100 --screenshot=/tmp/tfx.ppm
```

Direct missions normally inherit Arcade permissions and replenished stores,
independently of the numerical flight model. `--mission=501 --career-rules`
instead supplies the original career rules for analytical flight comparisons;
it does not enrol a pilot or start a persistent campaign.

The presentation player runs original images, delta animations, captions and palette fades; Space finishes a caption or advances a scene, Escape skips the sequence, and Shift+Q exits. Original menu scripts can also run independently with `--menu`, including the animated main menu (`mastopts`). Other presentation graphs include `cdcolint`, `cdsomint`, `cdfwsint`, `cdyugint` and `cdyugmid`; `--language` selects briefing text. Presentations retain their original synthesised and CD music requests.

In the inspector, WASD moves, R/F rises/descends, arrows or right-drag look around and Shift moves faster. Release builds omit the developer options. No CI or automatic release process is configured.

```sh
ctest --test-dir build --output-on-failure
python3 tools/check_reference.py --build build --research ../tfx-reverse
```

The optional reference check compares every decoded archive entry byte for byte with the independently checked research extractor. Original game data remains external to the repository. The current executable-table reader requires the reference 2.05 `TFX.DAT` as well as `DID.DAT`.

The research tools and captures currently live in the sibling `tfx-reverse` directory. Native comparison commands, verified behaviour and current rendering limitations are in [implementation.md](implementation.md). The [verification matrix](verification.md) summarizes current coverage; the [closeout scope](implementation-plan.md) records intentional omissions and how to investigate future discrepancies.