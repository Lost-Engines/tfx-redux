# Flight recordings

Redux records the displayed palette-index image, its complete DAC palette and
the additive HUD coverage, including the cockpit and full-resolution MFD cameras.
Starting Alt+V preserves the chosen resolution, FOV, draw distance and detail
settings. Video is lossless and retains frame timing. Pauses and modal menus are
omitted; recording continues while unfocused. Audio is not recorded. The WNN
prelude still precedes automatic replay after a completed mission. A recording
can also be opened with `--replay FILE`; `--skip-intro` omits the prelude for that
direct playback.

## Redux video, version 2

`src/recording/video.*` implements streaming capture/playback. Only the current
and previous indexed frame need to be resident. Files use the existing chosen
filename, normally `video.rec`, and are recognised by their signature. They
are not DOS-compatible model-queue files. There is no 348-model, 1,000-resource,
32-bit file-offset or 16-bit frame-count limit. Files can be larger than the
original scene recordings, depending on resolution and image complexity.

All integers are little-endian. The 24-byte file header contains:

| Offset | Type | Meaning |
| --- | --- | --- |
| 0 | 8 bytes | `TFXRVID` followed by NUL |
| 8 | u32 | Version, currently 2 (version 1 remains readable) |
| 12 | u32 | Reserved, zero |
| 16 | u64 | Frame count, patched when closed |

Each frame has a 24-byte header, followed by 768 DAC bytes and the compressed
pixel payload:

| Offset | Type | Meaning |
| --- | --- | --- |
| 0 | u32 | Width |
| 4 | u32 | Height |
| 8 | u64 | Display duration in microseconds |
| 16 | u32 | Compressed payload length |
| 20 | u8 | 0: full indexed image; 1: XOR with previous indexed image |
| 21 | u8 | Version 2: 1 if an optical HUD payload follows the pixels; otherwise 0 |
| 22 | 2 bytes | Reserved, zero |

The dimensions can change between frames. An XOR frame must match the previous
frame's dimensions. The encoder chooses the shorter of full-image and XOR
compression. Each packet uses the ByteRun/PackBits convention: controls 0–127
copy the next 1–128 literal bytes; controls 129–255 repeat the next byte 128–2
times. Control 128 is reserved and rejected. Runs can cross scanline boundaries.
The decoded stream must contain exactly width × height indices.

When the optical flag is set, the indexed payload is followed by a u32 compressed
length and a second ByteRun stream containing exactly width × height HUD coverage
bytes. This layer is always complete, never XOR encoded. At presentation, each
RGB channel receives `round(palette[6] × coverage × 0.9 / 255)`, saturated at 255.
The original scene indices are unchanged. Version 1 requires all three bytes at
offsets 21–23 to be zero and has no optical payload.

The reader validates dimensions, allocation limits, durations, reserved fields,
packet lengths, frame counts and trailing data. Unit checks include changing
palettes/resolution, noisy literals, sparse deltas, malformed files and 65,537
streamed frames. The private-window check independently decodes captured files,
compares every pixel index, DAC byte and optical coverage sample against read-only observations of the
live frame, then compares every RGBA pixel submitted to the production replay
window. It also checks that main and MFD cameras keep the selected draw distance
and that recording does not change the main FOV.

```sh
ctest --test-dir build --output-on-failure -R '^recording\.'
python3 tools/check_redux_recording.py --data-dir /path/to/TFX
# Under ASan/GDB, allow slower polled flight input:
python3 tools/check_redux_recording.py --build build-san --data-dir /path/to/TFX --key-hold=.8
```

## Original DOS format and baseline research

The original codec and reader remain available for legacy playback and analytical
checks. The original-code comparisons below describe the TFX C++ baseline;
they do not assert that Redux Alt+V writes the old format.

The retail 2.05 Alt+V recorder stores model submissions rather than rendered
pixels or simulation inputs. Original C5AC0 opens the file, C5BEC appends a
frame, C5B3C closes it and patches its header, and C5DAC writes the resource
dictionary. C5FF0 reads a frame and C6178 remaps model handles for playback.
These are executable file offsets in the September 1994 reference.

The standalone original codec is implemented in `src/recording/file.*`.
`recording::recorder` and `recording::playback` retain the native model-queue
streaming format for research and legacy playback. Live Redux capture uses the
video format above; the playback wrapper detects either signature automatically.

The configuration menu's original filename field is editable. Its application
state survives menu and mission returns but is not added to `TFX.CFG`, whose
original writer has no such setting. The native key maps, shifted-case branch,
18-character limit, underscore cursor, backspace, horizontal scrolling and
Enter completion are retained. Nonprinting host function/navigation keys do
not insert embedded NULs into path strings. A separate original widget capture
compares 115 supported editing steps and all 64,000 dialog pixels; DOS mouse
and keyboard delivery, PC-speaker tone/delay and cursor hardware are declared
boundaries. A private-window check changes the name using real keyboard input
and verifies that Alt+V writes the resulting, case-sensitive host filename.

```sh
python3 tools/check_recording_name.py
python3 tools/check_window_configuration.py --data-dir /path/to/TFX
```

All numeric fields use little-endian byte order. The ten-byte file header is:

| Field | Type | Meaning |
| --- | --- | --- |
| Frame count | u16 | Number of complete recorded frames |
| File length | u32 | Final file size, patched on close |
| Dictionary offset | u32 | End of frame data/start of the resource dictionary |

Each frame contains the following fields, in this order. Arrays are stored
by field, rather than as interleaved model structures. `N` is limited to the
original 348 model slots.

| Field | Stored form |
| --- | --- |
| Reserved state | Eight bytes copied from DATA AC9C; no retail producer identified |
| View angles | Three u16 words from CODE 47C20/22/24 |
| Model count | u16 |
| Queue priorities | N signed words |
| Queue payload slots | N u16 words |
| Drawing path | N bytes: 40 unrotated, 44 rotated, 56 ground shadow |
| Initial shape state | N bytes, compressed with fill byte 0 |
| Colour-remap row | N bytes |
| Retained scale | N bytes, compressed with fill byte 1 |
| Resource handles | N u16 words |
| Model Euler angles | Three arrays of N words; each compressed with fill byte 0 |
| Camera-space model origins | Three arrays of N signed dwords |
| Animation parameters | Eight bytes per slot, compressed with fill byte 0 |

C5958 compresses a field as pairs of one-byte fill-run and literal-run lengths,
followed by the literal bytes. Each run stops at 255. A trailing fill-only run
still writes its zero literal count. Each compressed field starts with its
u16 encoded byte count; an empty field writes a zero length. Both original
codec directions use a 2 KiB temporary buffer. The C++ reader bounds every run
and field, and rejects overlong, truncated or inconsistent files.

The dictionary stores a u16 entry count, that many original u16 handles, a u32
total name-byte count, then NUL-terminated names for entries 2 onwards. The
original reader allocates room for 1,000 entries. Entries 0 and 1 have no stored
name; playback supplies `badshape.3` for entry 1. Handle lookup searches the
table backwards. Resource names and handles are distinct from archive hashes
or the SSINFO model-name index.

`tools/check_recording.py` hosts only DOS file operations in memory. It executes
the original writer, compressor, dictionary builder, length patching and
decompressor. Forty-eight complete files/96 frames agree byte for byte,
including empty/full queues, signed priorities, all three model paths,
255-byte run boundaries, sparse animation parameters, all retained scale
values, and dictionaries of up to 1,000 entries. The original frame reader
also restores every supplied queue field. No original installation is written.

```sh
python3 tools/check_recording.py --research ../tfx-reverse --build build
ctest --test-dir build --output-on-failure -R '^recording\.'
```

## Live capture and the designator's two world passes

`tools/check_recording_session.py` runs the actual original recorder and actual
C++ window loop. DOS file operations are hosted in capture-owned memory; host
input is supplied only at the declared device/timer boundary. The native and
C++ resource managers allocate independently, so saved handles are resolved to
model names before comparing the recorded frames. Every other recorded field,
including unsorted priority/slot order and retained constructor metadata, is
compared directly.

A 12-frame mission-501 run records eight complete scene queues while changing
camera and weapon selection. A 16-frame mission-601 run records eleven queues
across designator entry, zoom, designation and clearing. All saved fields and
all rendered pixels agree. A further 22-frame Realistic designator sequence
retains exact state/pixel agreement after the integration. Independent queue
comparisons still cover 512 sequences/180,751 allocations/149,241 draws, and
32 mixed constructor/delivery sequences cover another 11,872 submissions.

The recording comparison exposed a caller-order detail that the earlier
single-pass designator integration missed. Native 0x92144 leaves the installed
camera unchanged for viewport 19. The ordinary world pass draws and records
with that inherited camera, then the 0x92C24 callback installs the new gimbal
and paints a second world. This is not simply a previous recorded frame:
current actor/effect state still participates in the first pass. Sky, ordinary
transients, precipitation and seeker updates belong to that primary pass;
the secondary designator pass retains the original animation gate. The host
now follows both passes and retains the last installed world camera, including
MFD camera submissions.

The native recording marker uses its own fixed eight-column/six-row driver
font at CODE 509E5; substituting presentation font 2 or the proportional HUD
font produces different pixels. The marker is loaded from the original
executable and paints its original shadow and ink.

The streaming writer closes cleanly at 65,535 frames instead of allowing the
original word counter to wrap into an unplayable zero-frame file. I/O errors
are reported without aborting flight. These are explicit host protections.

```sh
python3 tools/check_recording_session.py --research ../tfx-reverse --build build
```

## Streaming playback and the post-flight wrapper

`recording::playback` reads the resource dictionary once and decodes one frame
at a time through a bounded buffer. It validates the file length, dictionary,
compressed fields and end of the frame stream. Resource names are normalised
to model basenames, as by native C93BC. Asset-free tests cover actual file
creation, full queues, successive frames, replacement, RAII finalisation,
missing directories and malformed/truncated streams.

Native 7B2F4 first plays `anim/wwn.256` and `anim/prtv2.256`, each once with a
40-retrace fade-in and a one-BIOS-tick frame delay. These introductions do not
poll keyboard input. It then restores the full-screen 3D viewport, uploads the
environment's infrared palette, clears pending make codes and reads the film.
Recorded frames run at four PIT ticks each (about 13.65 frames/second), with
the shadowed PLAY caption alternating each frame. Its driver font has eight
drawn columns and a nine-pixel advance, unlike the presentation fonts.

The recording stores view angles and camera-space model submissions, but not
camera height, scrolling ground phase, drawing seed, status/detail flags or
carried-model bindings. Original playback inherits these from the flight.
The live viewer accordingly retains the actual flight scene, renderer and
attachments; it runs no mission rules, aircraft AI or flight dynamics during
replay. Immediate scratch submissions are not recorded by the original and
are not invented during playback.

`tools/check_recording_playback.py` executes the original file reader, resource
remapping, horizon, queue flush and blinking caption in a retained original
flight context. Eight ordinary-camera and eleven designator-camera frames,
plus their infrared palettes, agree pixel for pixel. Animation prefaces,
display flips and real-time waiting are explicit boundaries of that pixel
comparison. The separate native animation codec/clock/fade comparisons cover
their implementation. A private-window campaign check uses normal controls to
record and complete Hop 1, observes both introductions and replay, then checks
debriefing and Hop 2's inherited campaign and flight state.

```sh
python3 tools/check_recording_playback.py --case camera
python3 tools/check_recording_playback.py --case designator
python3 tools/check_training_engagement.py --campaign --recording
```
