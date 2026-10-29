# TFX Redux

**TFX: Tactical Fighter Experiment**, rebuilt for modern computers with high-resolution graphics, longer view distances and a fully working 3D cockpit. Fly the Eurofighter, F-22 Raptor and F-117 Nighthawk through the original campaigns, with the original aircraft, scenery, soundtrack and flight dynamics.

[Screenshot gallery](https://lost-engines.github.io/tfx-redux/)

## Getting started

1. Download the [latest release](https://github.com/Lost-Engines/tfx-redux/releases/latest) for Linux, Windows or Intel macOS and extract it.
2. Run `scripts\fetch-assets.bat` on Windows, or `./scripts/fetch-assets.sh` on Linux/macOS, to get the game and sound files.
3. Launch `tfx.exe` on Windows or `./tfx` on Linux/macOS.

Redux prepares the game files on first launch, then opens pilot enrolment. You can also use your own TFX 2.05 installation. See the [installation guide](docs/installation.md) for data locations and alternative setups.

## Improvements

- **High-resolution, widescreen graphics.** Play at your window’s resolution, including 4K, while retaining TFX’s original artwork and visual style.
- **Much longer view distances.** See cities, mountains, fleets and aircraft far beyond the original horizon, with adjustable detail and an option to draw the entire theatre.
- **Adjustable field of view.** Change your viewing angle, resolution and draw distance from the in-game Graphics menu.
- **A fully working virtual cockpit.** Live radar, maps, weapon displays, radio messages and warning lights replace the original static instruments.
- **Mouse look inside the cockpit.** With keyboard or joystick flight controls, look freely around the canopy, down at the instruments or over your shoulder.
- **A sharp, glowing HUD.** Smooth lines and lettering stay clear at high resolutions. In the virtual cockpit, the display sits on the HUD glass as you look around.
- **Free external cameras.** Orbit aircraft, missiles and targets with the mouse and move closer or farther away with the wheel. The laser designator also supports mouse aiming and wheel zoom.
- **Easier weapon loading.** Inspect and rotate a full-resolution aircraft, then click its pylons to place weapons. Leaving the arming screen no longer launches you straight into a mission by accident.
- **Pause and compose a shot.** Change views, look around and zoom while the action is frozen, without a pause label covering the image. Flight recordings preserve the view and resolution you were playing with.
- **More convenient controls.** Escape opens the flight menu, ejection asks for confirmation, and switching to another application lets a long flight continue in the background.

## Fixes to original-game bugs

- Fast mouse movement at low frame rates no longer causes wildly exaggerated pitch or roll inputs.
- Enabling wind now keeps it enabled when starting a mission or returning from rearming.
- Distant aircraft no longer appear in the wrong position in the missile-nose camera.

## Playing

Press **Escape → Graphics** to adjust the view, or open the controls and realism menus to set up your aircraft and input device. **F1** selects the classic 2D cockpit; **F10** selects the virtual cockpit. **F2/F3/F4** look left, right and behind; **F5** gives an external view. Use **1**, **2** and **3** to cycle the cockpit displays, and **P** or **Pause** to pause.

**Alt+V** starts or stops a flight recording. See [recordings](docs/recording.md) for playback options.

The [controls guide](docs/controls.md) covers weapons, autopilot, cameras, joystick setup and graphics options. The original game manual remains the best guide to the missions and flying the aircraft.

## Sound

Listen to the CD soundtrack or the synthesised versions through emulated Sound Blaster FM, Roland LAPC-I, SC-55 and SCC-1A hardware. Sound Blaster and Gravis effects are supported too, along with restored QSound spatial effects through our [QSound implementation](https://github.com/Lost-Engines/qsound-cpp).

The setup helper supplies the necessary sound files. You can also use your own CD image or local FLAC, Ogg Vorbis, MP3 or WAV soundtrack. See [sound options](docs/controls.md#sound).

## Building from source

Requires CMake 3.30+, a C++26 compiler and standard library, and OpenGL development libraries. On Debian/Ubuntu, install `libglfw3-dev` and `libgl1-mesa-dev`; CMake can also build GLFW automatically. The current flight backend requires an x86 processor.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
./scripts/fetch-assets.sh
./build/tfx
```

## Development

Redux builds on the [TFX C++ reconstruction](https://github.com/Lost-Engines/tfx-cpp) of the September 1994 release, version 2.05. For the research and developer tools, see:

- [Verification and tests](docs/verification.md)
- [Builds and releases](docs/build-release.md)
- [Implementation notes](docs/implementation.md)
- [Rendering performance](docs/rendering-performance.md)
- [TFX Traced](https://lost-engines.github.io/tfx-traced/)

GPL-3.0-or-later; see [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md).
