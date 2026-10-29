# Angular precision in Redux

The original sine table has 1,024 samples per turn. Cosine uses the same table
with a quarter-turn offset. Stored 16-bit angles are shifted right by six before
lookup, giving steps of 0.3515625 degrees. At 3,840 pixels wide and a 56-degree
horizontal field of view, a step near the centre of the image is about 22 pixels.

There is a second reduction: the native camera converts an aircraft's Q15
orientation basis back to 1,024-phase Euler angles, filters those integer angles,
then looks up a new view matrix. The high-resolution renderer previously inherited
both reductions. Continuous mouse-look offsets did not restore the lost aircraft
or camera precision.

Redux now retains separate display transforms using double-precision
`std::sin`, `std::cos` and `std::atan2`:

- Cockpit and head/chase views use the stored aircraft axes without first
  rounding them to native angle phases. Q15 length drift is normalised away.
- Head/chase follow angles retain fractional values through the existing
  quarter/eighth follow factors and tracking-roll behavior. Differences wrap
  across the 0/360-degree boundary. Fixed camera angles use all sixteen bits.
- Camera setbacks follow the continuous orientation, including chase, missile,
  impact, ejection and target views. Fly-past and tactical bearings use `atan2`
  without first shrinking their coordinate differences to signed words. The
  fly-past camera also retains fractional target positions.
- The designator camera uses all sixteen angle bits. The vector HUD receives
  the aircraft's stored basis and keeps the pitch-ladder projection in floating
  point through to rasterisation, avoiding native depth and pixel rounding.
- Moving models, scenery components and nested rotated model helpers use
  continuous sine/cosine evaluation in the enhanced scene pass. The player model
  and the HUD glass use the same aircraft basis, keeping the canopy and attached display registered.

Redux also uses standard trig in every remaining runtime caller: aircraft
rotation and movement, AI steering, target bearings, designator intersections,
landing guidance, flak placement, raster instruments, palette lighting and the
fixed-point model/camera interfaces. `std::atan2` replaces the original tangent
threshold table. World bearings accept full coordinate differences instead of
first shrinking them to signed words. Rotation accumulators apply single angle
units instead of waiting for a 64-unit table step; fractional remainders are
retained. The orbit calculation no longer adds the old one-sample complement
offset.

The fixed-point interfaces still store Q15 amplitudes and 16-bit angles. The
new helpers evaluate `std::sin` and `std::cos` with all angle bits, then convert
the result to the existing 32767 amplitude scale. Both sine peaks now have equal
magnitude. This removes table quantisation without changing the game's coordinate
units, simulation timing, turn limits or authored animation states. It does not
replace every fixed-point calculation with floating point.

Simulation outputs can therefore differ numerically from the original, including
trajectories and decisions near thresholds. The faithful `tfx-cpp` retains the
original arithmetic and its native verification. Redux has no runtime sine,
cosine or arctangent lookup table; historical copies in
`tests/legacy_trigonometry.h` are used only by the comparison benchmark.

This does not interpolate the entire simulation between updates or invent
intermediate authored animation states. An actor supplied only with already
rounded angles cannot regain lost information. Low frame rates, fixed simulation
steps and uneven video sampling can therefore still produce visible stepping.
Offline video should sample its camera and aircraft pose at a consistent cadence;
alternating one and two simulation ticks per 30fps image adds another source of
uneven movement independently of the angular tables.

## Standard functions or larger tables?

Measured on 29 October 2026 with GCC 16.2.0, C++26, `RelWithDebInfo` (`-O2`,
`-ffp-contract=off`, no fast-math), on an Intel Xeon E5-2699A v4. The benchmark
was pinned to one available logical CPU. Times are local measurements, not
portable speed guarantees.

The asset-free `trigonometry_benchmark` uses 65,536 deterministic nonconstant
angles in [−π, π], eight passes per measurement, and the median of seven runs.
Each sine/cosine case also evaluates the nine rotation-matrix components.
Lookup tables are initialised outside the timed loop; range reduction and
interpolation are included. Accuracy uses 131,073 samples around the full cycle.

| Sine/cosine implementation | ns per pair | ns per rotation | Maximum absolute error versus double standard functions |
| --- | ---: | ---: | ---: |
| Original 1,024-phase Q15 table | 7.39 | 29.39 | 0.00616 |
| `std::sin` + `std::cos`, double | 23.23 | 73.52 | Reference |
| Vectorstorm `sincos_any`, double | 23.50 | 75.09 | Same results as reference |
| Standard functions, float | 12.41 | 39.10 | 1.29 × 10⁻⁷ |
| 65,536-entry double table, phase rounded down | 7.25 | 26.28 | 9.59 × 10⁻⁵ |
| 4,096-entry double table, linear interpolation | 8.88 | 30.02 | 2.94 × 10⁻⁷ |
| 16,384-entry double table, linear interpolation | 9.08 | 31.49 | 1.84 × 10⁻⁸ |

The original-table error here includes rounding arbitrary input angles down to
a table phase. It is different from the much smaller amplitude error *at the
stored sample angles*. “Reference” means the double library result, not an exact
symbolic evaluation of sine.

Vectorstorm's bundled `sincos.h` calls the compiler/library paired sine/cosine
routine. It is not an approximation. GCC can combine adjacent standard sine and
cosine calls too. Its `sqrt_fast.h` contains square-root and reciprocal-square-root
approximations, which are separate from trigonometry. On compilers without the
generic double builtin, the benchmark explicitly labels its standard-function
fallback rather than reporting that as a Vectorstorm result.

For `atan2`, the same 65,536 directions give:

| Implementation | ns per bearing | Maximum angular error versus `std::atan2` |
| --- | ---: | ---: |
| Native signed-word direction/phase conversion | 55.47 | 0.263° |
| `std::atan2`, double | 29.54 | Reference |
| 4,096-interval ratio table, linear interpolation and octant reconstruction | 11.53 | 0.000000277° |

These are the complete benchmark wrappers, including conversion into native
signed-word inputs where applicable, rather than isolated assembly instruction
costs.

A table is faster in the small sine/cosine test, but the renderer is dominated
by other work. A second experiment substituted the 4,096-entry interpolated
implementation for the display sine/cosine helper in a private build. Nine
alternating pairs of runs rendered 48 frames each at 3,840 × 2,160, with a
32-cell draw distance and LOD scale zero. Each frame contained 1,021 model parts
and 62,205 primitives; warm-up, archive loading and screenshots were excluded.

| Display trig backend | Median complete frame | Range of run means | Median scene drawing |
| --- | ---: | ---: | ---: |
| Double standard functions | 42.69 ms | 41.75–47.30 ms | 39.77 ms |
| 4,096-entry interpolated table | 43.18 ms | 42.12–46.21 ms | 40.19 ms |

Both produced the same indexed-image hash for that scene. The overlapping
measurements show no useful whole-frame win from the table; they do not establish
that standard trig is intrinsically faster. Redux therefore uses the standard
double functions for these display paths. The tested lookup approximations stay
in the benchmark, with no new runtime setting or approximation dependency.

```sh
cmake --build build --target trigonometry_benchmark redux_render_benchmark -j
build/trigonometry_benchmark
build/redux_render_benchmark /path/to/TFX 32 48 3840 2160
```

The first command measures all mathematical candidates. The normal renderer
benchmark uses the selected standard-function implementation; the temporary
A/B build changed only `render::sin_cos` to the same 4,096-entry interpolation
algorithm in `tests/trigonometry_benchmark.cpp`.

See also the [square-root and normalisation measurements](square-root-benchmarks.md)
for the separate comparison of Vectorstorm's approximation modes.

## Verification

The full-circle `trigonometry_tests` checks every stored angle against independent
long-double functions, including symmetry, cardinal values, bearing quadrants,
fixed-point matrix geometry, sub-phase movement, ground targeting, orbit alignment
and fractional military-flight rotations.

The current suite has 225 tests. Forty inherited comparisons now have
Redux-specific expectations in the same fixture files: every input remains
unchanged, while the expected numerical outputs reflect the finer trig. Their
CTest names use `redux_`; the original evidence remains in `tfx-cpp` and this
repository's history. The manifest records hashes of both versions. These
snapshots guard against later regressions; they are not independent proof of
the new maths, which is checked by the full-circle and geometry tests above.

The fixed-point camera's maximum adjacent-frame movement in the 4K slow-turn
check is now 0.442585 pixels, alongside 0.442467 pixels for the floating display
camera. The former table-based path reached 22.159 pixels in that test.

Five alternating before/after pairs of the 4K virtual-HUD benchmark (24 warmed
frames each, 32-cell distance, same CPU/compiler as above) measured median totals
of 46.94 ms before and 46.02 ms after. HUD time was 3.75/3.77 ms. This does not
establish a speedup; it shows no rendering penalty in this bounded workload.
The real-asset HUD and all-six-cockpits probes also pass.

`tools/record_redux_trig_fixtures.py` audits the snapshot values without writing
them. Its explicit `--record` mode first requires the independent trig and
continuous-camera checks to pass, then records only expectation tokens read by
the probes. Malformed fixtures and other probe failures remain errors; normal
CTest runs always compare the stored outputs exactly. Do not record new values
without reviewing the reason for their changes.

### Earlier display-only verification

The following original-code results were recorded before the runtime trig
replacement. They describe the inherited baseline; they are not claims of
byte-exact Redux simulation after the change.


`continuous_pose_tests` advances the aircraft basis in single angle-word steps
through several table boundaries. At 4K / 56 degrees, the largest adjacent-frame
landmark movement is 0.443 pixels with the display path, versus 22.159 pixels
with the native path. It also checks handedness, orthonormality, wraparound,
vertical attitudes, rearward transitions, tracking roll and repeated paused reads.
Additional checks exercise sub-phase camera setbacks, fractional fly-past
movement, the lost-missile handoff, designator rotation, nested model helpers,
and the HUD's level/banked geometry and negative-pitch wrap.

All 224 CTest cases pass. The native camera harness still matches 12,288
pose/matrix updates across 24 cockpit, head/chase, missile and impact modes,
including split physics/render bases, refuelling model selection, aperture and
horizon handoffs. Another 1,024 original HUD frames and 1,024 designator
controller/overlay cases agree. The nested-helper comparison matches all
submitted vertices and VGA pixels in 1,022 completed original frames; two cases
fault in the original divide instruction and are reported separately. Its probe
now handles those expected projection aborts instead of stopping the batch; that
test-only fix is shared with `tfx-cpp` through the upstream merge.

The live instrument probe covers all six cockpit models at left, centre and
right views, including 4K and wide fields of view.

```sh
cmake --build build -j
ctest --test-dir build --output-on-failure
python3 tools/check_view_cameras.py --model-submission --refuelling --projection --horizon --split-basis
python3 tools/check_attitude_display.py
python3 tools/check_designator.py
python3 tools/check_model_articulation.py
build/redux_hud_probe /path/to/TFX
build/virtual_cockpit_probe /path/to/TFX
```

The native check requires the analysis Python environment and reference data;
the regular CTest suite does not require proprietary game assets.
