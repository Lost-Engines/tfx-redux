#!/usr/bin/env python3
"""Check recording, replay and high-resolution MFDs on a private display.

GDB only reads pixels and camera parameters. Independently decode the on-disk
video, compare every index and palette with the live displayed frame, then
compare every RGBA pixel sent to the actual playback window.
"""
import argparse
import json
import math
import os
from pathlib import Path
import select
import struct
import subprocess
import tempfile
import time
from window_input import confirm_flight_quit

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=Path('build'))
parser.add_argument('--data-dir', type=Path, required=True)
parser.add_argument('--save-directory', type=Path, help='save the first recorded frame as a PNG for review')
parser.add_argument('--resolution', default='1001x603', help='render dimensions, including fractional UI scales')
parser.add_argument('--key-hold', type=float, default=.25)
args = parser.parse_args()
width, height = map(int, args.resolution.lower().split('x'))
resolution = (width, height)

def unpack(data, length):
    at = 0
    pixels = bytearray()
    while at < len(data):
        control = data[at]
        at += 1
        assert control != 128
        if control < 128:
            size = control + 1
            pixels.extend(data[at:at + size])
            at += size
        else:
            pixels.extend(bytes([data[at]]) * (257 - control))
            at += 1
    assert at == len(data) and len(pixels) == length
    return pixels

def decode(path):
    data = path.read_bytes()
    version, reserved = struct.unpack_from('<II', data, 8)
    assert data[:8] == b'TFXRVID\0' and version in (1, 2) and reserved == 0
    count, = struct.unpack_from('<Q', data, 16)
    at = 24
    previous = None
    result = []
    for _ in range(count):
        width, height, duration, length, mode, optical = struct.unpack_from('<IIQIBB', data, at)
        assert data[at + 22:at + 24] == bytes(2) and optical in (0, 1) and 0 < duration <= 600000000
        assert version == 2 or optical == 0
        at += 24
        palette = data[at:at + 768]
        at += 768
        pixels = unpack(data[at:at + length], width * height)
        at += length
        if mode:
            assert mode == 1 and previous is not None and len(previous) == len(pixels)
            pixels = bytearray(a ^ b for a, b in zip(pixels, previous))
        previous = pixels
        coverage = b''
        if optical:
            length, = struct.unpack_from('<I', data, at)
            at += 4
            coverage = bytes(unpack(data[at:at + length], width * height))
            at += length
        result.append((width, height, palette, bytes(pixels), coverage))
    assert at == len(data) and count >= 3
    return result

def colourise(palette, pixels, coverage):
    colours = [bytes(((component & 63) * 255 + 31) // 63 for component in palette[at:at + 3]) + b'\xff' for at in range(0, 768, 3)]
    result = bytearray(b''.join(colours[pixel] for pixel in pixels))
    for i, alpha in enumerate(coverage):
        if alpha:
            for channel in range(3):
                light = (colours[6][channel] * alpha * 9 + 1275) // 2550
                result[i * 4 + channel] = min(255, result[i * 4 + channel] + light)
    return result

with tempfile.TemporaryDirectory(prefix='tfx-redux-recording-') as temporary:
    root = Path(temporary)
    install = root / 'TFX'
    install.mkdir()
    for name in ('TFX.DAT', 'DID.DAT'):
        (install / name).symlink_to((args.data_dir / name).resolve())
    config = b'CONTROL=0\r\nMODEL=2\r\nSOUNDFX=0\r\nMUSIC=0\r\nREALISM=0,0,1,1,0,0,0\r\n'
    (install / 'TFX.CFG').write_bytes(config)
    read, write = os.pipe()
    display = subprocess.Popen(['Xvfb', '-displayfd', str(write), '-screen', '0', f'{max(width, 1024)}x{max(height, 768)}x24', '-nolisten', 'tcp'], pass_fds=(write,), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.close(write)
    game = None
    try:
        assert select.select([read], [], [], 5)[0], 'private display did not start'
        number = os.read(read, 64).decode().strip()
        os.close(read)
        env = dict(os.environ, DISPLAY=':' + number, LIBGL_ALWAYS_SOFTWARE='1', XDG_RUNTIME_DIR=temporary, XMODIFIERS='@im=none', UBSAN_OPTIONS='halt_on_error=1')
        for name in ('WAYLAND_DISPLAY', 'XDG_SESSION_TYPE', 'DBUS_SESSION_BUS_ADDRESS'):
            env.pop(name, None)
        env['ASAN_OPTIONS'] = env.get('ASAN_OPTIONS', '') + ':detect_leaks=0'
        def run(*command):
            return subprocess.run(command, env=env, check=True, capture_output=True, text=True, timeout=5)
        def tap(key):
            run('xdotool', 'keydown', key)
            time.sleep(args.key_hold)
            run('xdotool', 'keyup', key)
            time.sleep(args.key_hold)
        def wait(predicate, message, timeout=30):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline and game.poll() is None:
                if predicate():
                    return
                time.sleep(.05)
            raise RuntimeError(message + '\n' + log.read_text()[-6000:])
        observer = root / 'observe.gdb'
        observer.write_text('''set pagination off
set confirm off
set startup-with-shell off
set print thread-events off
python
from pathlib import Path
import json
root=Path(''' + repr(str(root)) + ''')
recorded=0
end
break tfx::world::scene::draw
commands
silent
printf "REDUX_VIEW %u %d %d %u %.9f %.9f\\n", pass, renderer.output.size.x, renderer.output.size.y, radius, view.focal_distance, view.far_distance
continue
end
break tfx::cockpit::hud_selection::cycle_left
commands
silent
printf "REDUX_SELECT L %u\\n", this->left
continue
end
break tfx::cockpit::hud_selection::cycle_right
commands
silent
printf "REDUX_SELECT R %u\\n", this->right
continue
end
break tfx::recording::video_recorder::append
commands
silent
python
start=int(gdb.parse_and_eval('image.pixels._M_impl._M_start'))
end=int(gdb.parse_and_eval('image.pixels._M_impl._M_finish'))
(root/f'live-{recorded}.pixels').write_bytes(gdb.selected_inferior().read_memory(start,end-start))
start=int(gdb.parse_and_eval('image.hud_coverage._M_impl._M_start'))
end=int(gdb.parse_and_eval('image.hud_coverage._M_impl._M_finish'))
(root/f'live-{recorded}.hud').write_bytes(gdb.selected_inferior().read_memory(start,end-start) if end>start else b'')
palette=int(gdb.parse_and_eval('&palette._M_elems[0]'))
(root/f'live-{recorded}.palette').write_bytes(gdb.selected_inferior().read_memory(palette,768))
print(f'REDUX_FRAME {recorded}')
recorded+=1
end
continue
end
run
''')
        log = root / 'game.log'
        executable = str((args.build / 'tfx').resolve())
        with log.open('w') as output:
            game = subprocess.Popen(['gdb', '--quiet', '--batch', '-x', str(observer), '--args', executable, '--data-dir', str(install), '--mission=703', '--resolution=' + args.resolution, '--fov=100', '--draw-distance=64', '--scale=1'], env=env, stdout=output, stderr=subprocess.STDOUT)
            def windows():
                found = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', 'TFX Redux'], env=env, capture_output=True, text=True, timeout=5)
                return found.stdout.splitlines()
            wait(windows, 'flight window did not open')
            window = windows()[0]
            run('xdotool', 'windowfocus', '--sync', window)
            wait(lambda: 'REDUX_VIEW ' in log.read_text(), 'flight did not render')
            for side, key in (('L', '1'), ('R', '3')):
                for _ in range(10):
                    tap(key)
                    entries = [line for line in log.read_text().splitlines() if line.startswith('REDUX_SELECT ' + side)]
                    if entries and entries[-1].endswith(' 5'):
                        break
                else:
                    raise RuntimeError('could not select both MFD cameras')
            tap('alt+v')
            wait(lambda: log.read_text().count('REDUX_FRAME ') >= 5, 'flight recorder did not capture frames')
            tap('alt+v')
            time.sleep(.4)
            tap('shift+q')
            confirm_flight_quit(env, window)
            game.wait(timeout=20)
        text = log.read_text()
        assert game.returncode == 0 and 'exited normally' in text and 'Cannot record' not in text and 'Sanitizer' not in text and 'runtime error:' not in text, text[-5000:]
        views = [line.split()[1:] for line in text.splitlines() if line.startswith('REDUX_VIEW ')]
        expected_focal = width / (2 * math.tan(math.radians(50)))
        assert views and all(int(radius) == 64 for _, _, _, radius, _, _ in views), 'recording or MFD retained the old draw distance'
        main = [view for view in views if view[0] == '0']
        assert all((int(w), int(h)) == resolution and abs(float(focal) - expected_focal) < .001 for _, w, h, _, focal, _ in main), 'recording changed the main resolution or FOV'
        instruments = [view for view in views if view[0] == '1']
        assert instruments and all(int(w) > 51 and int(h) > 35 and math.isclose(float(focal), 256 * min(width / 320, height / 200)) for _, w, h, _, focal, _ in instruments), 'MFD did not render at displayed resolution with its authored lens'
        frames = decode(install / 'video.rec')
        assert len(frames) == text.count('REDUX_FRAME ')
        for index, (stored_width, stored_height, palette, pixels, coverage) in enumerate(frames):
            assert (stored_width, stored_height) == resolution
            assert coverage == (root / f'live-{index}.hud').read_bytes() and any(coverage), ('optical HUD differs or is missing', index)
            assert pixels == (root / f'live-{index}.pixels').read_bytes(), ('recorded pixels differ', index)
            assert palette == (root / f'live-{index}.palette').read_bytes(), ('recorded palette differs', index)
        if args.save_directory:
            from PIL import Image
            args.save_directory.mkdir(parents=True, exist_ok=True)
            width, height, palette, pixels, coverage = frames[0]
            image = Image.frombytes('RGBA', (width, height), bytes(colourise(palette, pixels, coverage)))
            image.convert('RGB').save(args.save_directory / 'mfd-recording.png')
        assert (install / 'TFX.CFG').read_bytes() == config
        # Read back through the actual production playback window. Break at
        # entry so the SysV AMD64 span arguments remain in their ABI registers;
        # GCC can otherwise optimise their DWARF locations out under ASan.
        symbols = run('nm', '--defined-only', '--extern-only', '--format=posix', executable).stdout
        present_symbol = next(line.split()[0] for line in symbols.splitlines() if line.startswith('_ZN3tfx8platform6window7presentE') and line.split()[1] == 'T')
        observer.write_text('''set pagination off
set confirm off
set startup-with-shell off
set print thread-events off
python
from pathlib import Path
import struct
root=Path(''' + repr(str(root)) + ''')
played=0
end
break *''' + present_symbol + '''
commands
silent
python
assert 'x86-64' in gdb.selected_frame().architecture().name(), 'playback observer requires the native x87/AMD64 build'
dimensions=int(gdb.parse_and_eval('$rcx'))
width,height=struct.unpack('<ii',gdb.selected_inferior().read_memory(dimensions,8))
start=int(gdb.parse_and_eval('$rsi'))
(root/f'played-{played}.rgba').write_bytes(gdb.selected_inferior().read_memory(start,width*height*4))
print(f'REDUX_PLAY {played} {width} {height}')
played+=1
end
continue
end
run
''')
        log = root / 'playback.log'
        with log.open('w') as output:
            game = subprocess.Popen(['gdb', '--quiet', '--batch', '-x', str(observer), '--args', executable, '--data-dir', str(install), '--replay', str(install / 'video.rec'), '--skip-intro', '--resolution=' + args.resolution, '--scale=1'], env=env, stdout=output, stderr=subprocess.STDOUT)
            wait(windows, 'playback window did not open')
            run('xdotool', 'windowfocus', '--sync', windows()[0])
            game.wait(timeout=60)
        text = log.read_text()
        assert game.returncode == 0 and 'exited normally' in text and text.count('REDUX_PLAY ') == len(frames), text[-6000:]
        for index, (_, _, palette, pixels, coverage) in enumerate(frames):
            rgba = colourise(palette, pixels, coverage)
            assert rgba == (root / f'played-{index}.rgba').read_bytes(), ('playback changed displayed pixels', index)
        print(f'Live Redux capture/replay: {len(frames)} full-resolution frames match every palette index, DAC value, optical HUD sample and displayed RGBA pixel; FOV/range unchanged, both MFD cameras rendered at displayed resolution')
    finally:
        for process in (game, display):
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
