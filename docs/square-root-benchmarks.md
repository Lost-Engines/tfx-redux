# Square roots and vector normalisation

The current renderer keeps standard-library square roots. Vectorstorm's
approximations can evaluate independent inputs faster, but they did not improve
the measured rendering workloads. No approximate square-root mode is enabled by
this experiment.

## Method

Measurements taken on 29 October 2026, Intel Xeon E5-2699A v4, GCC 16.2.0,
C++26, RelWithDebInfo (`-O2`, `-ffp-contract=off`, no fast-math). Processes were
pinned to one available logical CPU. These are local CPU measurements, not
portable performance guarantees.

[`square_root_benchmark.cpp`](../tests/square_root_benchmark.cpp) calls the
bundled Vectorstorm routines directly. It covers both `float` and `double`,
square roots, inverse square roots, and three-component normalisation:

- 65,536 deterministic, nonconstant positive inputs spanning 2⁻²⁴ to 2⁴⁸.
- Throughput: eight passes, four checksum accumulators, median of nine runs.
- Serial dependence: four passes, each result contributing to the next input,
  also median of nine runs. This is a dependent calculation, not an isolated
  CPU instruction latency measurement.
- Accuracy: those inputs plus a 262,145-point mantissa sweep over [1, 4], covering
  both exponent parities. Errors are relative to double-precision standard
  functions, not an exact symbolic reference.
- Separate edge checks for signed zeros, minimum subnormal/normal values,
  maximum finite values, infinity, negative input and NaN.

The additional SSE-refined candidate uses Vectorstorm's float reciprocal-root
seed with one Newton step in the caller's precision. It is an experimental
composition, not a separate routine supplied by Vectorstorm. It falls back to
standard functions outside the positive normal float range. SSE candidates are
only included on supported x86 builds.

## Scalar results

Times are nanoseconds per evaluation, including the benchmark loop and checksum.
Independent work and dependent work behave differently; the throughput column
alone does not predict rendering cost.

| Precision / method | Root throughput | Root with serial dependence | Inverse-root throughput | Maximum relative root error |
| --- | ---: | ---: | ---: | ---: |
| float / standard | 1.51 | 6.17 | 2.37 | 5.96 × 10⁻⁸ |
| float / Vectorstorm fast | 1.08 | 12.35 | 1.00 | 4.75 × 10⁻⁶ |
| float / Vectorstorm coarse | 0.76 | 8.67 | 0.71 | 0.001751 |
| float / Vectorstorm SSE | 1.13 | 5.29 | 0.96 | 0.0003243 |
| float / guarded SSE + Newton step | 2.49 | 8.82 | 2.39 | 2.81 × 10⁻⁷ |
| double / standard | 2.40 | 8.06 | 3.92 | Reference |
| double / Vectorstorm fast | 1.57 | 12.10 | 1.39 | 4.60 × 10⁻⁶ |
| double / Vectorstorm coarse | 0.90 | 8.13 | 0.84 | 0.001751 |
| double / guarded float SSE seed + double Newton step | 3.00 | 12.42 | 2.76 | 1.59 × 10⁻⁷ |

`fast` makes two Newton refinements; `coarse` makes one. The double error is
about 0.00046% for `fast` and 0.175% for `coarse`. These figures apply to positive
normal inputs in the tested range. The routines are not general substitutes for
`std::sqrt`: for example, raw `sqrt_sse(0)` returns NaN, the bit-seeded inverse
routines return a large finite number for zero, and subnormal inputs can have
large relative errors. The HUD SSE experiment therefore includes range checks
and a standard-function fallback, including at zero-length pixel distances.

Disassembly of the measured HUD confirms that the standard positive-distance
path uses the CPU's `sqrtsd` instruction. The fast/coarse variants replace that
with their refinement arithmetic; the guarded SSE variant uses `rsqrtss` plus
a standard-root fallback. The name “fast” does not imply a shorter dependent
calculation on this CPU.

## Normalisation

Vectorstorm's `normalise_copy<mode>()` divides each component by its computed
length. The benchmark also tests multiplying by one reciprocal length. For
double-precision vectors:

| Method | ns per vector | Maximum unit-length error |
| --- | ---: | ---: |
| Vectorstorm standard mode | 6.83 | 4.44 × 10⁻¹⁶ |
| Vectorstorm fast mode | 6.72 | 4.60 × 10⁻⁶ |
| Vectorstorm coarse mode | 5.20 | 0.001754 |
| Multiply by `1 / std::sqrt(dot(v,v))` | 4.35 | 4.44 × 10⁻¹⁶ |
| Multiply by Vectorstorm fast inverse root | 5.53 | 4.60 × 10⁻⁶ |
| Multiply by Vectorstorm coarse inverse root | 4.44 | 0.001751 |
| Multiply by guarded SSE-refined inverse root | 5.74 | 1.58 × 10⁻⁷ |

The standard reciprocal is competitive with even the coarse approximation while
retaining double precision. Float normalisation was 5.13 ns in standard mode,
8.03 ns in fast mode and 6.62 ns in coarse mode. Raw float SSE multiplication
was 3.95 ns, with 0.000324 unit-length error. There is no reason here to reduce
the precision of the camera basis; normalisation is also a small part of the
frame's work. Existing normalisation callers retain their standard mode.

## Actual 4K drawing

The renderer comparison changes only the square root used for distance from a
HUD pixel to its stroke. It retains all projection, clipping, input coordinates,
coverage rounding and compositing. Each candidate is built separately with the
same flags, and the standard implementation is restored after the comparison.

Nine runs per candidate and HUD mode, with shuffled candidate order, render
24 warmed frames each at 3840 × 2160. The benchmark uses mission 501 terrain,
a 32-cell draw distance, LOD scale zero, three HUD modes and changing attitude.
The virtual HUD also changes head direction and projects onto the HUD glass.
Only the glass projection is included: this is not a full canopy/MFD benchmark.
Loading, simulation, audio, colour conversion and window upload are excluded.

| HUD distance root | 2D HUD, median ms | Virtual HUD, median ms | Scene + UI + 2D HUD, median ms | Scene + UI + virtual HUD, median ms |
| --- | ---: | ---: | ---: | ---: |
| Standard double | 5.22 | 3.82 | 49.68 | 48.90 |
| Vectorstorm double fast | 5.90 | 4.18 | 51.32 | 49.67 |
| Vectorstorm double coarse | 5.58 | 3.99 | 50.71 | 49.21 |
| Standard float | 5.30 | 3.82 | 50.29 | 49.13 |
| Vectorstorm float SSE, guarded | 5.51 | 3.95 | 51.12 | 48.46 |

Only the HUD calculation changes, so the isolated HUD timing is the useful
comparison. The apparent improvement in the SSE total comes from variation in
unchanged scene drawing; its HUD is slower. Standard-double 2D HUD run means
were 5.13–5.42 ms, versus 5.87–6.01 ms for fast mode. Virtual HUD run means were
3.79–3.95 ms standard versus 4.10–4.28 ms fast. Standard float was effectively
tied in the virtual view and did not establish a useful overall improvement.

The indexed world image is identical in all variants. Comparing the final
8,294,400-byte HUD coverage layers gives:

| Method | Changed 2D pixels | Changed virtual pixels | Largest coverage change, out of 255 |
| --- | ---: | ---: | ---: |
| Fast | 4 | 4 | 1 |
| Coarse | 10,459 | 3,882 | 2 |
| Standard float | 0 | 1 | 1 |
| Guarded SSE | 440 | 392 | 1 |

These are small edge-intensity differences, not corrupt images. The main reason
for retaining standard double precision is that the approximations did not
make this workload faster.

## Implementation and reproduction

The bundled bit-seeded routines used pointer casts to read floating-point bits
as integers and back. GCC warned about those reads when the benchmark began
instantiating them. They now use `std::bit_cast`, retaining the same constants,
iterations and arithmetic without violating C++ aliasing rules. No global
fast-math or aliasing compiler flags were changed. The production game still
uses standard square roots and standard Vectorstorm normalisation; simulation
and the faithful reconstruction are unaffected.

```sh
cmake --build build --target square_root_benchmark redux_render_benchmark -j
build/square_root_benchmark
build/redux_render_benchmark /path/to/TFX 32 24 3840 2160 --hud
build/redux_render_benchmark /path/to/TFX 32 24 3840 2160 --virtual-hud
```

The scalar benchmark needs no game data. The renderer uses the installed game
selected by its first argument. An optional output PPM also writes a `.coverage`
file when a HUD is enabled, allowing comparisons of the actual light mask.
The renderer normally uses standard double maths; the private A/B builds
replaced the pixel-distance `.length()` in `vector_hud::stroke` with the chosen
Vectorstorm mode, float standard root, or guarded SSE root. No extra game setting
is introduced. Measurement outputs and screenshots stay in the local build
folder.

The full Redux build, all 224 CTest cases, and the vector HUD rendering probe
passed with the standard implementation restored.
