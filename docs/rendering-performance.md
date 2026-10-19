# Rendering performance

The Redux benchmark isolates warmed rendering from loading, flight simulation,
audio and desktop presentation. It uses mission 501 terrain with a fixed elevated
camera, the highest-detail models and a representative opaque UI region. It is
not a complete-game FPS measurement.

Measurements on GCC 16.2, RelWithDebInfo (`-O2`), 1280×800:

| Whole-map frame stage | Before | After |
| --- | ---: | ---: |
| Horizon | 20.2 ms | 20.4 ms |
| Scene | 190.1 ms | 120.5 ms |
| UI compositing | 4.3 ms | 1.2 ms |
| Total | 214.6 ms | 142.1 ms |

The benchmark draws 4,385 model parts / 278,966 primitives. Its indexed-image
FNV-1a hash remains `17725320125237863711`; the before/after PPM exports also
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
