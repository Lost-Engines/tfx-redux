# Screenshot gallery

The local gallery builder captures Redux at **3840 × 2160** using the ordinary flight renderer. It covers the three flyable aircraft, 2D and virtual cockpits, navigation and weapon HUDs, weather, formations, cities, mountains and offshore scenery. Terrain, models, palettes and instrument drawing all come from the selected TFX installation.

The scenes are photographic staging: aircraft positions, camera poses and some formations are arranged for composition. They are not recordings of unscripted combat. The gallery records this distinction alongside the capture settings. Native 2D cockpit artwork retains its original resolution; the world, virtual cockpit geometry and vector HUD render at the requested output resolution. There is no image upscaling or retouching.

## Capture

Use a Debug or RelWithDebInfo build (the latter keeps developer tools enabled in this project), Python 3 and Pillow:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j --target tfx
python3 tools/capture_showcase.py --data-dir /path/to/TFX
```

Open `captures/showcase-4k/index.html` directly in a browser. The gallery works offline. Filters separate classic cockpits, virtual cockpits, formations, external views and scenery; click an image for the viewer, then use the arrow keys to browse. Each entry links to its lossless PNG and scene recipe. `contact-sheet.jpg` provides an overview; `manifest.json` records resolution, settings, Git revision, renderer hash, image hashes and whether tracked source changes were present.

The builder uses a temporary installation containing read-only asset links and its own preferences. It does not alter the selected installation’s preferences or pilot saves, play audio, open a game window, fetch assets, upload anything or publish a website. `captures/` is ignored by Git. The PNG library is a separate local resource, not part of the source distribution.

For quick composition checks:

```sh
python3 tools/capture_showcase.py --data-dir /path/to/TFX --preview
python3 tools/capture_showcase.py --data-dir /path/to/TFX --only 13-efa-mountain-bank
```

Previews render at 960 × 540 in a separate `previews/` directory. They never replace the 4K images. `--output`, `--manifest` and `--build` select other destinations, scene sets and binaries. Re-rendering a selection updates those entries and retains the others.

## Scene recipes

The editable inventory is [tools/showcase/scenes.json](../tools/showcase/scenes.json). Each entry specifies mission, environment, FOV, draw distance, LOD scale, caption and a line-based recipe. The renderer accepts these through:

```sh
./build/tfx --data-dir /path/to/private/TFX --mission 801 \
  --environment yugos_d --resolution 3840x2160 --fov 55 \
  --capture-scene /path/to/pose.scene --screenshot /tmp/frame.ppm
```

`--capture-scene` requires a direct mission and screenshot destination. It cannot be combined with inspection or simulation-step options, and is absent from Release builds. Recipes accept comments beginning with `#` and reject unknown keys, missing values, invalid numbers and trailing arguments.

| Field | Arguments | Meaning |
| --- | --- | --- |
| `position` | X Y Z | Exact player position in native world units; X/Z inside the 200 × 200 map, Y between −8192 and 0. Negative Y is above ground. One cell spans 2048 units. |
| `angles` | pitch heading roll | Native aircraft angles, expressed in degrees. |
| `aircraft` | `efa`, `f117`, `f22` | Choose the flyable aircraft. Mission stores remain authored. |
| `speed` | Mach factor | Override the mission’s basic-flight launch factor (0.05–2). Military-spec uses its own aerodynamic launch speed. |
| `view` | `cockpit`, `virtual`, `external`, `left`, `right`, `rear` | F1 panel, F10 virtual cockpit, external aircraft view, or native F2/F3/F4 head views. |
| `look` | yaw pitch | Virtual-cockpit head offset in degrees, positive right/down. |
| `camera` | eye-X eye-Y eye-Z target-X target-Y target-Z roll | External look-at camera, with roll in degrees. Eye and target must differ; exactly vertical views are rejected. |
| `hud` | `default`, `nav`, `guns`, `air`, `ground` | Select the actual weapon/navigation display. |
| `displays` | left centre right | Native MFD selections: 0–8, 0–2, 0–8. The ordinary aircraft/weapon rules still apply. |
| `actor` | SSD X Y Z pitch heading roll entry | Add an original SSD assembly at a staged pose; optionally invoke an entry after initialisation (0 means initializer only). Up to 64 actors. |
| `player-event` | entry | Invoke a native SSD entry on the player model. Repeat for multiple entries. |
| `at` | frame `player-event` entry | Invoke a native player event at a particular warmup frame. |
| `at` | frame `actor-event` actor-index entry | Invoke an event on a staged actor (zero-based recipe order). |
| `at` | frame `terrain-event` cell-X cell-Z entry | Invoke an event on an authored terrain assembly. |
| `mission-actors` | 0 or 1 | Hide/show the mission’s authored moving actors and fixed scenery; terrain and explicitly staged actors are retained. Default 1. |
| `hide-player` | 0 or 1 | Omit the player model for scenery and enemy-formation studies. Default 0. |
| `warmup` | frames | Advance scenery/model animation and camera/instrument caches before the final still (default 24, maximum 500). |

The debug path overrides the native tile-centre placement, performs one ordinary flight update to initialise airdata and sensors, then restores the requested player pose. It rejects a colliding/ejected spawn. Subsequent warmup frames hold aircraft positions while advancing native scenery and effect animation at four timer ticks per frame. Staged SSD actors are visual subjects, not newly registered AI combatants; they do not add radar contacts or participate in mission scoring. The cockpit’s existing mission contacts still drive its instruments.

Use a new scene ID for an alternative composition rather than overwriting a selected finished image. The gallery contains no post-processing controls: improve the camera or scene, then render again.

## Catching a particular animation state

Timed events run before the numbered warmup frame. Frame 0 is before the first animation update; frame `warmup` is immediately before the final still. Events on the same frame retain recipe order. Events beyond the final frame, missing actor indices and cells outside the map are rejected. SSD entry numbers remain specific to the selected resource; the recipe does not reinterpret them as universal actions.

For example, this poses a MiG-29 with its original vortex and engine animation streams:

```text
actor mig_29 121856 -650 47104 -15 0 65 15
at 0 actor-event 0 9
warmup 30
```

The MiG-29, MiG-21 and F-16 SSDs use entries 15/16 to start and finish their parameter-6 vortex sequence. The original AI manoeuvre bytecode invokes those events explicitly. They are not a universal G threshold: the Eurofighter and Nighthawk use parameter 6 for their braking parachutes, and other model variants differ again. A staged still demonstrates the authored effect, not an unscripted aerodynamic measurement.

For ordnance in flight use the weapon table's **fired** SSD (for example `xgbu_24b` or `xagm_109`). The similarly named loadout presentation models have a different scale. Match the selected aircraft to a mission with a compatible loadout; changing `aircraft` preserves the mission's authored pylon indices.

Keep additional scene inventories beside their local output, for example `captures/showcase-4k/scene-library.json`, and pass them with `--manifest`. Screenshots, contact sheets, captions and expanded composition recipes belong to the separate media library, not Git. Copy the finished gallery directory to shared storage as one unit; it needs no server or game installation to browse.
