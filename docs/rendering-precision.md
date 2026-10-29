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

Redux now retains a separate display orientation:

- Cockpit and head/chase views use the stored aircraft axes without first
  rounding them to native angle phases. Q15 length drift is normalised away.
- Head/chase follow angles retain fractional values through the existing
  quarter/eighth follow factors and tracking-roll behavior. Differences wrap
  across the 0/360-degree boundary. Fixed camera angles use all sixteen bits.
- Moving-model and component transforms use continuous sine/cosine evaluation
  in the enhanced scene pass. The player model and the HUD glass use the same
  aircraft basis, keeping the canopy and attached display registered.

The native camera pose, word matrices, smoothing state, model selection and
projection metadata are still calculated as before. Physics, actor rules,
original-code probes and the faithful `tfx-cpp` renderer keep their original
arithmetic. This is a presentation change, not a change to aircraft handling.

This does not interpolate the entire simulation between updates or invent
intermediate authored animation states. An actor supplied only with already
rounded angles cannot regain lost information. Low frame rates, fixed simulation
steps and uneven video sampling can therefore still produce visible stepping.
Offline video should sample its camera and aircraft pose at a consistent cadence;
alternating one and two simulation ticks per 30fps image adds another source of
uneven movement independently of the angular tables.

## Verification

`continuous_pose_tests` advances the aircraft basis in single angle-word steps
through several table boundaries. At 4K / 56 degrees, the largest adjacent-frame
landmark movement is 0.443 pixels with the display path, versus 22.159 pixels
with the native path. It also checks handedness, orthonormality, wraparound,
vertical attitudes, rearward transitions, tracking roll and repeated paused reads.

All 224 CTest cases pass. The native camera harness still matches 12,288
pose/matrix updates across 24 cockpit, head/chase, missile and impact modes,
including split physics/render bases, refuelling model selection, aperture and
horizon handoffs. The live instrument probe covers all six cockpit models at
left, centre and right views, including 4K and wide fields of view.

```sh
cmake --build build -j
ctest --test-dir build --output-on-failure
python3 tools/check_view_cameras.py --model-submission --refuelling --projection --horizon --split-basis
build/virtual_cockpit_probe /path/to/TFX
```

The native check requires the analysis Python environment and reference data;
the regular CTest suite does not require proprietary game assets.
