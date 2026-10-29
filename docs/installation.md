# Installation and game data

Download and extract the package for your operating system from
[Releases](https://github.com/Lost-Engines/tfx-redux/releases/latest).
Windows uses `tfx.exe`; Linux and macOS use `tfx`. The macOS download targets
Intel Macs running macOS 15 or newer. Linux packages are built on Ubuntu 24.04.

## Fetch the game and sound files

On Windows, run `scripts\fetch-assets.bat`. On Linux or macOS, run:

```sh
./scripts/fetch-assets.sh
./tfx
```

The helper fetches the reference CD images (about 576 MiB) and the optional
Roland synthesiser ROMs. Run it again to check the files and replace any missing
or damaged copies. It does not replace pilot saves or preferences.

On first launch, Redux prepares the game files directly from the downloaded CD.
No DOS installer or separate archive extractor is needed. Later launches can
repair missing or changed game files while preserving your progress.

The default data location is `~/.local/share/tfx` on Linux/macOS, or
`$XDG_DATA_HOME/tfx` if set. Windows keeps the files beside `tfx.exe`.
The helper and game use the same location. A game installation in the current
working directory takes precedence. An optional destination argument to the
helper and `--data-dir /path/to/TFX` select a different location.

The Unix helper needs `curl`, `unzip` and `sha256sum` or `shasum`. The Windows
helper uses the built-in `curl.exe`, `certutil.exe` and `tar.exe`.

## Use an existing installation

Redux uses the September 1994 CD release, TFX 2.05. Select your installation with:

```sh
./tfx --data-dir /path/to/TFX
```

It needs `TFX.DAT` and `DID.DAT`. Keep `START.EXE` for synthesised music and
`GUS.COM` for Gravis effects. The directory must be writable for pilot saves
and preferences. Existing `TFX.CFG` choices are retained.

Graphics preferences are saved separately in `REDUX.CFG` beside `TFX.CFG`.
The Graphics menu has a Defaults button; removing or renaming `REDUX.CFG` also
resets the Redux graphics options. This does not remove your pilot saves.

## Music and optional hardware sounds

The downloaded CD supplies the soundtrack. The helper also fetches
CM-32L/LAPC-I, SC-55 v1.21 and SCC-1A v1.30 ROMs for synthesised music.
Redux finds these automatically in the installation directory; `--rom-dir`
is available if you keep them elsewhere. Existing music choices are preserved.

Your own CUE/BIN disc image or local FLAC, Ogg Vorbis, MP3 or WAV rips can also
supply the soundtrack. See [sound options](controls.md#sound) for device
selection and [CD soundtrack sources](controls.md#cd-soundtrack-sources) for
track naming and command-line options.
