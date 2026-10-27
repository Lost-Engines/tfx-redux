# TFX Redux

An enhanced native C++26 version of Digital Image Design’s **TFX: Tactical Fighter Experiment**, based on the [TFX C++ reconstruction](https://github.com/Lost-Engines/tfx-cpp) of the September 1994 CD release, **version 2.05**. Redux adds higher-resolution rendering, configurable views and arming-screen improvements while retaining the original game data, flight models and mission logic.

The supplied fetch script downloads the reference game CD and audio firmware. You can also use your own TFX 2.05 installation. Research exports are neither required nor bundled.

## Getting started

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
./scripts/fetch-assets.sh
./build/tfx
```

Requires CMake 3.30+, a compiler and standard library with C++26 support, and OpenGL development libraries. CMake uses an installed GLFW 3.4+ or fetches a pinned copy. On Debian/Ubuntu, install `libglfw3-dev` and `libgl1-mesa-dev` for the desktop dependencies. The faithful numerical flight backend currently requires x87 support.

The default installation directory is `~/.local/share/tfx` on Linux/macOS
(`$XDG_DATA_HOME/tfx` when set), or beside `tfx.exe` on Windows. The fetch
script uses that location too. An installation in the current directory takes
precedence; `--data-dir` selects a different location.

On first launch, the fetched disc supplies the required game files automatically. Later launches also repair missing or changed files from that disc, preserving settings and pilot saves. No DOS installer or 7-Zip extractor is needed.

An existing installation can be selected with `--data-dir /path/to/TFX`. It needs the reference `TFX.DAT` and `DID.DAT`. Keep `START.EXE`
there for synthesised music, and `GUS.COM` for Gravis effects. Existing `TFX.CFG`
settings are read on startup. Pilot saves and configuration changes use the
selected installation directory, which should be writable.

Normal startup plays the introduction and opens pilot enrolment. `--skip-intro` goes straight to enrolment. To try a mission directly:

```sh
./build/tfx --mission=501
```

Mission 501 is the first Fighter Weapons School engagement. Direct launches use the mission’s preset stores; campaign launches open the original arming screen. Choose **DEFAULT** for its preset, then **OK** to fly.

## Playing

The original menus, briefings, mission planning, arming, flight, combat and results are connected. All four flight models are available, along with the original cockpit/MFDs, cameras, weather, damage, refuelling and automatic landing. Landing and stopping can refuel, repair and rearm your aircraft through the original arming screen. Campaign flights return to the authored presentations and retain DOS-compatible `TFXSAV.0`–`TFXSAV.9` pilot files.

Arcade uses mouse steering. The other flight models support keyboard and joystick controls; `--flight-model` and `--control` override the saved choices. Press **Escape** (or **backtick**) in flight for the menu and realism settings, and **P** or **Pause** to pause. Choose **EJECT** and confirm, or press **Ctrl+Escape twice** to eject; **Shift+Q** opens the quit/enrol confirmation.

**Alt+V** starts or stops a flight recording. Its default filename is `video.rec`, editable in the configuration menu. A new recording replaces that file. After a campaign flight, the original WNN sequence and recorded flight play automatically.

The floating window fits the desktop automatically, with square pixels and nearest-neighbour presentation. `--scale=2` starts a 640×400 window, or doubles an explicitly selected render size. Losing focus releases all flight input; flight, presentations and replay keep running. Use **P**, **Pause** or the menu when you want to pause. Most ordinary keys, including Enter and Space, resume; the resume press is consumed until release. F-key camera changes and screenshots remain available while paused, with no pause label over the image. External views retain mouse orbit and wheel zoom while paused, and the Z designator view retains mouse aiming and wheel zoom.

See [controls and configuration](docs/controls.md) for weapons, autopilot, cameras, MFDs, joystick bindings, recordings and developer commands. The original manual remains the best guide to flying the missions.

## Redux improvements

By default, 3D rendering uses the **window’s actual pixel dimensions**, including after resizing. The other defaults are the original **64.01° horizontal FOV**, a **16-cell draw radius** and **LOD scale 0.1**. For example:

```sh
./build/tfx --resolution=1920x1080 --fov=90 --draw-distance=64
./build/tfx --resolution=1280x800 --draw-distance=infinite
```

- `--resolution WIDTHxHEIGHT` fixes the 3D render resolution, including aircraft and weapons in the arming screen. Without it, both follow the window’s framebuffer size. Widescreen and other aspect ratios use square pixels.
- `--fov DEGREES` sets the horizontal field of view, from 10° to 170°. The main camera uses this angle. The missile-nose camera retains its wider view; the HUD pitch ladder and target markers follow the selected projection.
- `--draw-distance CELLS` controls the terrain radius; `0` or `infinite` removes the far plane and includes the whole finite map. The original radius is 8 cells. Whole-map rendering can be expensive in the software renderer.
- `--lod-scale FACTOR` controls visual detail cutoffs. The default, `0.1`, postpones detail reductions to ten times their original distance. `0` keeps the most detailed models and disables drawing-distance tests in scenery scripts; `1` restores the original thresholds. Missile smoke retains its authored distance-dependent particle sizes at every setting. Gameplay and collision ranges are unaffected.

These display settings are command-line options for now.

The following departures from the original are intentional:

- World rendering uses a wider coordinate range, an unrestricted visible-model queue and resolution-independent clipping and projection. Terrain, buildings, aircraft and effects can remain visible beyond the original limits. The original palette colours, quantised lighting and authored model drawing order remain in use; exact DOS pixel matching is no longer the rendering target.
- Distance shading extends across the selected draw distance (the map diagonal for whole-map views), retaining the original palette lookup rather than introducing blended colours. Sky and ground bands are projected for the actual view instead of relying on the original narrow-view shortcuts.
- Cockpit artwork, MFD lettering, menus and presentations retain their original pixel detail and use nearest-neighbour scaling. Cockpits stay centred, with black margins where needed. Embedded MFD scenes render at their displayed pixel size and use the chosen draw distance and LOD settings, while retaining their authored camera framing.
- The forward HUD uses full-resolution, antialiased strokes and circular sights, including the pitch ladder and lettering based on the original glyphs. Its green light adds to the scenery underneath, with thinner strokes and smooth subpixel motion for the pitch ladder and heading scale.
- Arming picks the aircraft's pylon volumes along the cursor ray, including after rotating the aircraft. The held weapon follows that cursor ray instead of mapping mouse movement to fixed aircraft axes. Buttons use the actual cursor position, without the original miniature-aircraft hotspot offset.
- Escape cancels an active arming operation first. At the idle preflight arming screen, it returns to the preceding choice screen (or pilot enrolment when none exists). During an in-flight rearm, it asks before returning to the mission. Cancelling restores the loadout held before opening the arming screen.
- Distant point models use the same missile-view projection as polygons. Two-pixel models are clipped at the screen edge instead of wrapping a pixel onto the next row.
- At higher resolutions, point lights use smaller circles that shrink with distance and account for the camera’s field of view. Distant aircraft keep a visible minimum size (about two pixels across at 1080p, three at 4K), including when LOD is disabled. Lights are hidden by nearer scenery rather than the drawing order of whole terrain tiles.
- Missile smoke uses consistent spacing between puffs even when the frame rate changes. Its visibility follows the selected field of view, and its animation and particle sizes are independent of mesh LOD.
- Moving the mouse in external views orbits the aircraft, missile or target being viewed, with mouse capture and no simultaneous mouse steering. The wheel moves the camera closer or farther away without changing the field of view. Both work while paused. Selecting a camera again restores its normal angle and distance.
- In the Z laser-designator view, moving the mouse aims the camera and the wheel zooms, including while paused. A locked target continues tracking until X releases it. Mouse input cannot steer or throttle the aircraft in this view; joystick flight controls remain available. Entering or leaving the view clears retained mouse steering.
- Escape opens the flight menu; ejection has a separate confirmation or a two-press Ctrl+Escape shortcut. Focus loss releases controls without pausing flight or playback.
- Looking steeply down no longer paints a blue circular patch beneath the camera. The software horizon uses faster span drawing while retaining its palette bands.
- In-flight screenshots capture the full render resolution and declare square pixels in their ILBM metadata. Images with the optical HUD use true colour to preserve its blending.
- Pixel clipping includes the complete render target; MFD target reticles stay within their display. Rendering also avoids unnecessary polygon allocation and clipping work without changing the palette or drawing style.

**Flight recordings:** Alt+V records the view as displayed, including its resolution, FOV, distance shading, cockpit and MFDs. Recording preserves your rendering settings. Replay uses the recorded colours and viewpoint instead of the original infrared model-queue replay. New files use a compressed, lossless Redux format; existing DOS recordings remain playable. Recordings contain video only and omit pauses and menus. They continue while the window is unfocused.

To replay a saved flight directly:

```sh
./build/tfx --replay /path/to/video.rec --skip-intro
```

Omit `--skip-intro` to include the original WNN prelude. [Recording details](docs/recording.md) describe both file formats.

## Sound

Sampled Sound Blaster and Gravis effects and speech, FM music, Roland LAPC-I and Sound Canvas emulation are connected. The saved `SOUNDFX=4` setting selects Gravis; `SOUNDFX=2` with `MUSIC=2` selects the original LAPC-I synthesised flight effects. `--music=soundblaster-fm` selects synthesised FM music explicitly; `--music=none` disables music. Roland modes find matching ROMs automatically beside the game files or in the default installation directory. Details are in [controls and configuration](docs/controls.md).

QSound spatial processing is built in and enabled automatically for Sound Blaster effects. It needs no firmware download. `--qsound=off` selects ordinary mono output; `--qsound=on` explicitly requires Sound Blaster effects to be selected. This uses the recovered original filters with endpoint-clamped positioning; exact sound-card arithmetic remains unverified. Music and CD playback are unaffected.

To fetch or repair the game disc and audio firmware on Linux or macOS:

```sh
./scripts/fetch-assets.sh
./build/tfx --music=roland-scc1a
```

On Windows, run `scripts\fetch-assets.bat`. The scripts use the default
installation directory described above, so no `--rom-dir` option is needed.
They fetch the reference CD as individual BIN files (roughly 576 MiB), plus
CM-32L/LAPC-I, SC-55 v1.21 and SCC-1A v1.30 ROMs. Every file is checked on each
run; missing or damaged files are downloaded again, with retries. An optional destination argument and
`--rom-dir` are available for custom locations. Linux/macOS
needs `curl`, `unzip` and `sha256sum` or `shasum`; Windows uses its built-in
`curl.exe`, `certutil.exe` and `tar.exe`.

The fetched installation defaults to **CD music**, streamed directly from its
`cd/tfx.cue` and BIN files. An existing installation’s saved music choice is
retained; `--music=soundblaster-cd` (or `--music=cd`) selects CD playback.

To use another disc image or your own ripped tracks:

```sh
./build/tfx --cd-cue /path/to/tfx.cue
./build/tfx --cd-audio-dir /path/to/soundtrack
```

Rips can be FLAC, Ogg Vorbis, MP3 or WAV, named `track02.flac`, `track03.ogg`,
and so on, using the original physical CD track numbers (audio starts at 02).
Names such as `track-02` and uppercase extensions are also accepted. Playback
uses the original track selection, stop and flight-loop behaviour.

## Development and verification

The inherited TFX C++ baseline matched across all 128 retail mission setups and their first sixteen Arcade frames. Longer controlled comparisons cover successful interception and ground attack, incoming fire and aircraft loss, refuelling, mission return, ejection and recording. Subsystem checks execute the original code independently, with compact regressions runnable without game data:

```sh
ctest --test-dir build --output-on-failure
```

For a minimal installation, `cmake --install build-rel --component Runtime --prefix /path/to/output` copies the executable and the matching platform’s fetch script and asset manifests from a Release build. Original game data and ROMs remain separate.

The Redux regressions cover resolution, FOV, MFD framing, UI opacity, screenshots, lossless recording and the corrected distant-point projection. With the reference data available, the arming probe checks cursor-ray picking before and after rotation, and weapon attachment for all three aircraft:

```sh
python3 tools/check_redux_points.py
./build/redux_arming_probe /path/to/TFX /tmp/tfx-arming-check
python3 tools/check_redux_arming.py --data-dir /path/to/TFX
python3 tools/check_redux_recording.py --data-dir /path/to/TFX
```

[Renderer profiling](docs/rendering-performance.md) records the measured optimisation results and how to reproduce them.

The [verification matrix](docs/verification.md) records the inherited baseline’s complete-session evidence, persistent campaign checks and long-session audio/lifetime coverage; its original-renderer comparisons are historical evidence rather than a promise of identical Redux pixels. [Implementation notes](docs/implementation.md) retain the research details. The research tree is the sibling `tfx-reverse` directory.

GPL-3.0-or-later; see [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md).
