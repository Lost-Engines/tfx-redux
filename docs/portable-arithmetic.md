# Portable arithmetic

Redux uses `float` for the fields that were stored as single precision in TFX,
and `double` for intermediate calculations and double-precision state. This
applies to all four flight models, carried stores, damage, weather, cameras,
instruments, music selection and the music-rendering helper. Trigonometric
functions call the standard C++ library. The game no longer changes the x87
control word, executes x87 transcendental instructions or rejects other CPU
architectures.

The original constants, expression order, float storage boundaries and guarded
integer conversions remain. CMake keeps `-ffp-contract=off`, so compiling for
a CPU with fused multiply-add does not silently combine separate operations.
This change does not alter resource or save formats, fixed-point coordinates,
random-number generation or simulation timing.

Vectorstorm still supplies its upstream overloads for other numerical types.
Redux does not use those overloads. The source check covers our game, tests
and C++ tools so a future upstream merge cannot reintroduce extended-precision
types, literals or the removed x87 headers unnoticed.

## Reference checks

The pre-change Release build passed all 225 tests. Removing the x87-specific
environment test left 224: 213 passed without changing their comparisons.
Eleven numerical comparisons needed bounded floating-point comparisons because
standard-library trigonometry rounds differently from x87. Their fixture inputs
and expected values are unchanged.

`tests/numeric_fixture.h` preserves the type of each output. Integers, event
flags, seeds, stored angle words and state transitions still compare exactly.
Floating-point fields accept at most
`128 * double_epsilon * max(1, abs(expected), abs(actual))`, or four float
epsilons for a float field. That allows rounding and cancellation near zero;
finite values cannot match infinities or NaNs. Matching NaNs are compared by
classification because their payload bits are implementation-dependent.

On Linux x86-64 with GCC 16.2, the eleven fixtures had 258 changed floating
fields. The largest scaled difference was 19.48 double epsilons; the largest
absolute difference was 2.99e-8 in a large-magnitude field. No fixture was
regenerated to accept the new arithmetic. The comparator itself has tests for
integer exactness, error bounds, nonfinite values and malformed input.

A separate before/after run exercised 36 persistent flight scenarios: all three
aircraft, military-spec and realistic flight, ground and airborne starts, and
keyboard, mouse and joystick samples. Each ran 1,024 updates at 20 ms. Across
the 36,864 updates, every exported integer, random state and event matched.
The largest position difference was 1.82e-12 metres and the largest velocity
difference was 8.66e-14 m/s. These are bounded comparisons of this workload;
they do not promise identical trajectories across every platform and mission.

The suite now has 226 tests, including the numeric comparator and the source
portability check. Run the suite and inspect the numerical differences with:

```sh
ctest --test-dir build --output-on-failure
python3 tools/audit_numeric_fixtures.py --build build --report /tmp/numeric-audit.json
```

The audit runs the ordinary tests with diagnostics enabled. It does not relax
their comparisons or write fixtures. Debug and Release builds on Apple Silicon
are included in the normal build-and-release workflow alongside the Intel,
Linux and Windows builds.

## Capturing the original executable

The optional Python research tools still execute the original x86 program in
Unicorn. Some captures need a hardware x87 helper to correct Unicorn's handling
of transcendental instructions. That helper now comes from the faithful
`tfx-cpp` checkout, where those exact instruction results belong:

```sh
cmake --build ../tfx-cpp/build --target native_x87_backend
```

The capture tools also accept `TFX_NATIVE_X87_LIBRARY=/path/to/library.so` for
another build location. The Redux executable, packages and ordinary tests have
no dependency on this library or the sibling checkout.
