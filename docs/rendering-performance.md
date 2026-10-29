# Rendering performance

The Redux benchmark isolates warmed rendering from loading, flight simulation,
audio and desktop presentation. It uses mission 501 terrain with a fixed elevated
camera, the highest-detail models and a representative opaque UI region. It is
not a complete-game FPS measurement.

Earlier allocation/clipping measurements on GCC 16.2, RelWithDebInfo (`-O2`), 1280×800 (before the horizon and point-size changes below):

| Whole-map frame stage | Before | After |
| --- | ---: | ---: |
| Horizon | 20.2 ms | 20.4 ms |
| Scene | 190.1 ms | 120.5 ms |
| UI compositing | 4.3 ms | 1.2 ms |
| Total | 214.6 ms | 142.1 ms |

The benchmark draws 4,385 model parts / 278,966 primitives. At that revision, its indexed-image
FNV-1a hash remained `17725320125237863711`; the before/after PPM exports also
compare byte for byte. Wall-clock timings vary with system load.

A separate Callgrind run at a 64-cell radius, two measured frames, drops from
2,413,566,594 to 1,467,623,942 instructions (about 39%). Startup and two warm-up
frames are excluded. Allocation/deallocation and temporary vertex copies were
prominent costs. The changes:

- Reuse clipping, projection and scanline scratch storage rather than allocating
  vectors for each polygon and each clipping plane.
- Pass through wholly admitted polygons without rebuilding their vertex arrays;
  reject wholly excluded polygons before allocating clipping output.
- Reuse the transformed-vertex buffer across a model's primitives.
- Calculate UI column mappings once per composition instead of per output row.

The pixel arithmetic, authored paint order, palette lookups and stipple sequence
are preserved. These changes improve CPU rendering; full-map, high-resolution
views still involve substantial software work.

Reproduce the measurements:

```sh
cmake --build build --target redux_render_benchmark -j
./build/redux_render_benchmark /path/to/TFX 0 6 1280 800 /tmp/scene.ppm
valgrind --tool=callgrind --instr-atstart=no \
  --callgrind-out-file=/tmp/tfx-render.callgrind \
  ./build/redux_render_benchmark /path/to/TFX 64 2 1280 800
callgrind_annotate --auto=no /tmp/tfx-render.callgrind
```

Building with Valgrind headers enables the profiling-region markers; the
benchmark still builds and runs normally when those optional headers are absent.

## Horizon span drawing

At 1920×1080 with a 16-cell radius and LOD scale 0, the same warmed scene
(306 parts, 20,391 primitives) now measures:

| Stage | Per-pixel horizon | Span horizon |
| --- | ---: | ---: |
| Horizon | 41.24 ms | 0.49 ms |
| Scene | 12.52 ms | 12.28 ms |
| UI | 2.32 ms | 2.29 ms |
| Total | 56.08 ms | 15.06 ms |

This is about 3.7 times faster for the isolated renderer, without changing its
software pipeline. The horizon previously normalised a ray and tested palette
bands at every pixel. Each band's intersection with a scanline is quadratic;
solving those boundaries once lets us fill spans with the same palette index.
`redux_view_test` compares every pixel against the independent ray calculation
across banked, inverted, steep and high-altitude views. The arbitrary near-ground
colour-253 circle has been removed deliberately. Scaled point lights also change
the final image, so the earlier whole-image hash is historical.

At 3840×2160 the same scene takes about 32.6 ms (2.5 horizon, 20.9 geometry,
9.2 UI). These exclude simulation, audio, colour conversion and desktop upload;
they are not promised gameplay frame rates. Full-map visibility still incurs
much more geometry work. The benchmark explicitly holds LOD scale at 0, while
the game's new default of 0.1 can reduce that work further.


## Point visibility

Small point sprites now use a separate visibility pass against the projected
surfaces. Screen bins restrict each query to nearby polygons; polygon colours
still use the original painter order. This prevents a terrain tile's sorting
bias from erasing lights in front of it, without adding a depth operation to
every framebuffer pixel. Transparent texture texels and regular stipple gaps
remain transparent; procedural cloud/smoke stipple is not a solid occluder.

The same 1080p, 16-cell, full-detail benchmark above measured 0.47 ms horizon,
14.05 ms scene and 2.29 ms UI (16.80 ms total), with 306 parts / 20,391
primitives. The added visibility work costs about 1.8 ms in this scene. Timing
still excludes simulation, audio, colour conversion and desktop upload.


## Point sizes

Whole-object dots have a minimum diameter of the shorter framebuffer dimension
divided by 720, clamped to at least one pixel and rounded by the disc rasteriser.
That gives two pixels across at 1080p and three at 4K, independent of FOV.
Aircraft whose full-detail meshes become very small or thin use the same floor,
so disabling LOD cannot make a distant aircraft harder to spot than its dot.
This substitutes only for tiny geometry; ordinary aircraft silhouettes stay intact.

Lights retain the authored one/two-pixel distinction nearby. Their diameter is
scaled by the camera lens and by `800 / hypot(800, distance)` in world units
(800 is roughly two kilometres), approaching inverse-distance scaling without
an unbounded glow near the camera. The distant minimum is half the aircraft
minimum, clamped to one pixel. Thus remote city lights do not remain large discs
beside smaller geometry. Native 320×200 lights keep their original pixel shapes;
smoke retains its existing particle sizing and LOD-independent animation.


## Optical HUD

The forward HUD is rasterised directly into a coverage layer at the framebuffer
resolution. Lines use a distance-based edge filter, circles are tessellated at
screen resolution, and the small original glyphs supply connected stroke shapes.
The pitch ladder and heading strip retain fractional positions. Coverage takes
the maximum at joins, so repeated endpoints do not become brighter.

Only the HUD is blended after palette conversion: it adds 90% of palette colour 6,
modulated by coverage and saturated per RGB channel. The terrain and cockpit keep
their indexed colours. Later cockpit/menu writes occlude the layer, and the layer
is preserved in screenshots and recordings. Optical screenshots are 24-plane
true-colour ILBM files; ordinary indexed screenshots retain the original format.

The isolated three-layout HUD probe measured about 0.26 ms at 320×200, 0.81 ms at
1001×603 and 1.99 ms at 1920×1080, including coverage composition but excluding
final colour conversion. These are local CPU measurements, not gameplay rates.

## Virtual cockpit textures and live instruments

The continuous texture painter now writes clipped spans directly. Its ordinary
opaque mode avoids two general pixel calls, repeated clipping checks, and unused
pattern/coverage branches for every texel. UV interpolation, palette remapping,
transparency and painter order are unchanged. The general painter remains for
patterned polygons; the probe compares it pixel for pixel with the fast path.

An isolated F-22 canopy render, looking down at the full panel, measured:

| Resolution | Previous texture path | Direct spans |
| --- | ---: | ---: |
| 1920×1080 | 18.77 ms | 12.11 ms |
| 3840×2160 | 79.39 ms | 48.07 ms |

These are local CPU timings for an isolated model with facing disabled, excluding
world rendering, simulation and presentation. They show a 35–39% reduction in
that rendering cost, not an equivalent increase in overall game frame rate.
The indexed image hashes match before and after at both sizes.

The complete F1 panel and instrument renderer run once per frame on a small
320×200 canvas. MFDs, warning lamps, the radio ticker, engine indicators and
side readouts map to their corresponding regions in `global_1` and `global_2`,
only for the six front-canopy models. The port panel is in `global_2`; both the
centre display and message panel cross the seam. Fractional atlas coordinates
sample the original canvas directly, without first squeezing the graphics into
integer atlas texels. This removes the extra uneven pixel widths/heights while
keeping nearest-neighbour palette artwork. A cell lookup selects each mapping;
rectangular mapping scales are precomputed.

Instrument regions include the F1 idle artwork, so clearing a lamp or message
restores its background. Engine lamps and reheat pips refresh on each new
canvas without changing the shared native page counter. The radio ticker
advances once per frame and appears on the physical message panel. Replacement
pixels bypass model lighting, with black opaque; the rest of the cockpit keeps
its authored art and lighting. Normal and refuelling canopies share these UVs;
external aircraft and other users of the atlases are unaffected.

Camera MFDs render at their native screen dimensions before projection onto the
panel, avoiding oversized intermediate images. These additional world passes
sample scene animation without advancing it. The primary scene remains responsible
for advancing model effects. Paused displays use the existing copied instrument
state, so radar sweeps, map markers and camera orbits remain frozen.

```sh
cmake --build build --target virtual_cockpit_probe -j
./build/virtual_cockpit_probe /path/to/TFX /tmp/tfx-cockpits
python3 tools/check_window_virtual_cockpit.py --data-dir /path/to/TFX
```

The asset probe checks all six models, left/right mapping, the centre seam,
opaque black, palette handling, clipping and coverage, and prints static/live
canopy timings. The earlier atlas-copy implementation added roughly 1 ms
locally; direct source sampling trades additional per-pixel mapping work for
consistent instrument pixels. The probe reports current static/live costs;
active camera pages still require their own scene passes.

With the full panel and direct source sampling, the same isolated F-22 probe
measured 13.33 ms static / 18.47 ms live at 1080p, and 52.54 ms / 72.99 ms at
4K. These are canopy-only CPU timings, not whole-game frame rates; the more
accurate sampling adds about 5 ms / 20 ms respectively on this machine.

## 4K software-rendering pass (29 October 2026)

Redux keeps the software renderer. A future WebGPU/Wasm version is a separate
architecture project; this pass improves the current CPU pipeline.

Profiling a warmed 3840×2160 scene found that UI composition alone accounted for
38% of its executed instructions. Solid polygons still called the general pixel
painter for every pixel. The optical HUD also tested every pixel in a diagonal
stroke's bounding rectangle, making long banked lines especially expensive.

The changes are confined to drawing and presentation:

- Solid, unshaded polygon spans use contiguous fills after the existing clipping
  and facing decisions. Optional write coverage is filled over the same span.
- UI composition expands each covered native row once and copies its visible
  runs. HUD occlusion uses the same mapping. Fractional scales and letterboxing
  retain the original destination-to-source rounding.
- HUD strokes visit a conservative strip around the line on each scanline.
  Distance, antialiasing, end caps, glass clipping and overlap calculations are
  unchanged for every pixel that can receive light.
- Colour conversion expands the palette once, copies complete RGBA values and
  precomputes the 256 HUD light levels. Flight, arming and video playback reuse
  the window's colour buffer between frames.

Measured locally with GCC 16.2, RelWithDebInfo (`-O2`), against revision
`7a73d4b`. These are isolated component times, excluding simulation and window
upload; the rows overlap and must not be added together as a full-frame budget.

| 4K operation | Before | After |
| --- | ---: | ---: |
| World benchmark: horizon, geometry and UI | 34.8 ms | 16.5 ms |
| Static F-22 canopy | 50.8 ms | 21–23 ms |
| Live F-22 canopy | 69.7 ms | 41–44 ms |
| Native UI composition | 10.3 ms | 0.3 ms |
| Two long diagonal HUD strokes, including clear/copy | 186.9 ms | 3.4 ms |
| HUD occlusion, including coverage copy | 35.5 ms | 1.3 ms |
| Palette and additive HUD conversion | 24.8 ms | 9.8 ms |

The diagonal-stroke benchmark deliberately stresses the old bounding-rectangle
behaviour; ordinary short labels do not have that cost. Large textured cockpit
surfaces and very long draw distances still take substantial CPU time. These
numbers are not promised gameplay frame rates.

Reproduce the component measurements with the same installation and build:

```sh
cmake --build build --target redux_render_benchmark redux_composition_tests virtual_cockpit_probe -j
./build/redux_render_benchmark /path/to/TFX 16 30 3840 2160
./build/redux_composition_tests --benchmark /path/to/TFX
./build/virtual_cockpit_probe /path/to/TFX
```

`render.redux_composition` compares UI scaling and sparse masks against a simple
per-destination-pixel reference at native, reduced, fractional, portrait and 4K
sizes. It also checks all 256 palette indices against all 256 HUD coverage
values, allocation reuse, resizing and invalid coverage lengths. The HUD asset
probe compares horizontal, vertical, diagonal, reversed, clipped and zero-length
strokes against a full-area distance reference, plus sparse cockpit masks.
The existing texture probe covers all six ordinary/refuelling cockpit models.

Twelve complete 4K captures covering all three aircraft, F1/F10, turned and
banked cockpits, night scenery, weather, smoke and external views retain every
RGB pixel from the pre-change executable. Captures remain local under the
ignored build directory. Live private-window checks cover virtual MFDs, pause,
graphics changes and resizing. Recording/playback verification additionally
compares the indexed image, DAC palette, optical coverage and every displayed
RGBA pixel at 3840×2160.

## Square-root alternatives

The [square-root benchmarks](square-root-benchmarks.md) compare Vectorstorm's
fast, coarse and SSE routines with standard functions, including 4K HUD drawing
and pixel differences. The measured drawing paths retain standard square roots;
the approximations did not improve their performance.
