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
