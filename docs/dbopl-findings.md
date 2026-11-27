# DBOPL findings for external follow-up

Working note intentionally left uncommitted. These findings concern the pinned
DOSBox DBOPL revision used by TFX C++; they have not been checked against current
upstream and are not claimed to be previously unknown issues.

## Source and local fix

Upstream source: DOSBox Staging commit
`e164e788f9819d5ab898d705f863e2046baf8b03`,
[`src/hardware/dbopl.cpp`](https://github.com/dosbox-staging/dosbox-staging/blob/e164e788f9819d5ab898d705f863e2046baf8b03/src/hardware/dbopl.cpp).

TFX extracts the chip core, excluding the DOSBox mixer adapter, through
`cmake/dbopl.cmake`. Project vocabulary defines `Bit32s` as `int32_t`, `Bit32u`
as `uint32_t`, and `Bitu` as `uintptr_t`. The project uses C++26.
The local fix is TFX C++ commit `7d197dd`.

## 1. Member offsets calculated through null objects

`DBOPL::InitTables()` creates null `Chip*` and `Channel*` pointers and computes:

```cpp
reinterpret_cast<Bitu>(&(chip->chan[index]))
reinterpret_cast<Bitu>(&(chan->op[opNum]))
```

These are intended as byte offsets, but accessing the members through null
objects is undefined C++ behaviour. UBSan stops during real FM initialization:

```text
dbopl.cpp:1438:39: runtime error: member access within null pointer of type 'struct Chip'
```

The quoted line is from TFX's generated core before the local fix; its line
numbers need not equal upstream's. The second expression has the same problem.

The local adapter replaces the calculations with:

```cpp
static_assert(std::is_standard_layout_v<DBOPL::Chip>);
offsetof(DBOPL::Chip, chan) + index * sizeof(DBOPL::Channel)

static_assert(std::is_standard_layout_v<DBOPL::Channel>);
offsetof(DBOPL::Channel, op) + opNum * sizeof(DBOPL::Operator)
```

The assertions are important: `offsetof` requires a suitable layout. These
expressions preserve the intended byte offsets without null member access.
No audible defect or production crash has been established from this finding.

## 2. Signed overflow in attack-rate initialization

After fixing the offsets, strict UBSan reaches an overflowing multiplication
in the attack-rate search inside `DBOPL::Chip::Setup()`:

```cpp
guessAdd = ((guessAdd * mul) >> 12);
```

Observed diagnostic:

```text
dbopl.cpp:1280:27: runtime error: signed integer overflow: 4158 * 591053 cannot be represented in type 'int'
```

That line number includes the adapter's added headers. The arithmetic appears
to depend on signed 32-bit wraparound followed by arithmetic right shift.
Widening the product would change the calculation and is not an equivalent fix.

TFX now applies `-fwrapv` specifically to the isolated DBOPL target with GCC/
Clang. This defines signed addition, subtraction and multiplication wraparound;
it does not disable the other address/undefined-behaviour checks. An upstream
alternative would express the intended modulo arithmetic explicitly with
unsigned multiplication and conversion back to the signed result before the
right shift. That alternative has not been implemented or verified here.

## Reproduction and verification

Use the existing ASan/UBSan configuration, with `UBSAN_OPTIONS=halt_on_error=1`,
and construct the real `fm_synth`; the CD controller probe deliberately stops
FM register writes at a transport boundary and does not exercise this hardware.
For a current checkout containing the fix:

```sh
cmake --build build-san --target fm_synth_test music_player_probe -j3
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 build-san/fm_synth_test
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  build-san/music_player_probe /path/to/TFX/START.EXE /path/to/TFX/DID.DAT
python3 tools/check_fm_pcm.py
```

The sanitizer build uses `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer` for C++, and sanitizer flags at link time. Run outside
GDB/ptrace when using LeakSanitizer. To reproduce the first issue on the pinned
core, remove the adapter's `offsetof` replacements. To expose the second,
retain the offset fix but omit `-fwrapv`.

Before either fix, PCM SHA-256 and sequencer-state hashes were captured for
all 27 original FM resources. Each renders 256 shared IRQ ticks at 48 kHz,
volume 75, followed by a two-second release tail. After both fixes, all PCM
bytes and sequencer traces remain identical. Hashes, source digests and frame
counts are retained in `tests/reference/fm_pcm.json`; no game bytes are stored.
The original OSL register/timing comparisons remain separate evidence.

The asset-free test exercises initialization, non-silent finite dual-mono
output at 44.1 and 48 kHz, and exact output across callback partitions. The real
music lifecycle probe also passes ASan/UBSan and leak checks, with 2,832,000
PCM samples agreeing across callback sizes and start/mute/resume/replace/stop
transitions. This establishes preservation for the tested inputs, not proof
that every DBOPL path is free of undefined behaviour.

## Before external submission

- Check whether current upstream already fixes either construction or specifies
  signed-wrap compiler flags elsewhere in the normal DOSBox build.
- Reduce the reproduction to the standalone pinned chip core and its original
  type definitions, without TFX assets.
- Prefer an explicit arithmetic patch upstream if build flags are unsuitable;
  repeat PCM comparisons before claiming equivalence.
- Review the local adapter diff for a minimal patch suitable for upstream.
  No upstream issue or pull request has been submitted.
