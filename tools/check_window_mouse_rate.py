#!/usr/bin/env python3
"""Check mouse flight samples with deliberately slow presentation.

Uses a disposable silent install and private X11 display. GDB observes flight
input and delays presentation; XTest supplies actual pointer motion. Native
Arcade consumes every relative sample once. Redux's aerodynamic models must
never receive virtual-stick positions outside their supported range.
"""
import argparse
import ctypes
import ctypes.util
import os
from pathlib import Path
import re
import select
import signal
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=Path('build'))
parser.add_argument('--data-dir', type=Path, default=Path('../tfx-reverse/installs/cd_1994_09/TFX'))
parser.add_argument('--flight-model', choices=('arcade', 'military-spec', 'realistic'), default='arcade')
parser.add_argument('--fps', type=int, choices=(10,15,60), default=10)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='tfx-mouse-rate-') as temporary:
    root = Path(temporary)
    install = root / 'TFX'
    install.mkdir()
    for name in ('DID.DAT', 'TFX.DAT'):
        (install / name).symlink_to((args.data_dir / name).resolve())
    (install / 'TFX.CFG').write_text('CONTROL=0\nMODEL=0\nSOUNDFX=0\nMUSIC=0\nREALISM=0,0,1,1,0,0,0\nDETAIL=2\n')
    read_fd, write_fd = os.pipe()
    display = subprocess.Popen(['Xvfb', '-displayfd', str(write_fd), '-screen', '0', '1000x800x24', '-nolisten', 'tcp'], pass_fds=(write_fd,), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.close(write_fd)
    game = None
    try:
        assert select.select([read_fd], [], [], 5)[0], 'Xvfb did not start'
        number = os.read(read_fd, 64).decode().strip()
        os.close(read_fd)
        env = dict(os.environ, DISPLAY=':' + number, LIBGL_ALWAYS_SOFTWARE='1', XDG_SESSION_TYPE='x11')
        for name in ('WAYLAND_DISPLAY', 'DBUS_SESSION_BUS_ADDRESS'):
            env.pop(name, None)
        script, log = root / 'observe.gdb', root / 'game.log'
        script.write_text(r'''set pagination off
set confirm off
set startup-with-shell off
set print thread-events off
python
import time
mouse_deadline = time.monotonic()
end
break tfx::flight::player_flight::advance
commands
silent
printf "INPUT %d %d %d %d %d\n", context.basic_controls.pitch_delta, context.basic_controls.roll_delta, context.basic_controls.mouse_buttons, context.physics.input.mouse.x, context.physics.input.mouse.y
continue
end
break tfx::platform::window::present
commands
silent
printf "FRAME\n"
python
mouse_deadline = max(mouse_deadline + 1 / TFX_TEST_FPS, time.monotonic())
time.sleep(max(0, mouse_deadline - time.monotonic()))
end
continue
end
run
'''.replace('TFX_TEST_FPS', str(args.fps)))
        command = [str((args.build / 'tfx').resolve()), '--data-dir', str(install), '--mission', '601', '--music', 'none', '--control', 'mouse', '--flight-model', args.flight_model, '--scale', '2']
        command += ['--resolution', '640x400', '--scale', '1']
        with log.open('w') as output:
            game = subprocess.Popen(['gdb', '--batch', '-x', str(script), '--args', *command], env=env, cwd=root, stdout=output, stderr=subprocess.STDOUT, start_new_session=True)

            def run(*command):
                return subprocess.run(command, env=env, capture_output=True, text=True, check=True, timeout=5)

            def rows(label, since=0):
                return [tuple(map(int, line.split()[1:])) for line in log.read_text()[since:].splitlines() if line.startswith(label + ' ')]

            def wait(predicate, message, timeout=30):
                deadline = time.monotonic() + timeout
                while time.monotonic() < deadline:
                    if predicate():
                        return
                    if game.poll() is not None:
                        break
                    time.sleep(.05)
                raise AssertionError(message + '\n' + log.read_text()[-2500:])

            windows = []
            def ready():
                windows[:] = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', 'TFX (C|Redux)'], env=env, capture_output=True, text=True).stdout.splitlines()
                return bool(windows) and len(rows('INPUT')) > 3

            wait(ready, 'flight did not start')
            run('xdotool', 'windowfocus', '--sync', windows[0])

            def frames(number=3):
                start = log.read_text().splitlines().count('FRAME')
                wait(lambda: log.read_text().splitlines().count('FRAME') >= start + number, 'window stopped presenting')

            frames(6)
            start = len(log.read_text())
            xlib = ctypes.CDLL(ctypes.util.find_library('X11'))
            xtest = ctypes.CDLL(ctypes.util.find_library('Xtst'))
            xlib.XOpenDisplay.argtypes = [ctypes.c_char_p]
            xlib.XOpenDisplay.restype = ctypes.c_void_p
            xlib.XFlush.argtypes = [ctypes.c_void_p]
            xlib.XCloseDisplay.argtypes = [ctypes.c_void_p]
            xtest.XTestFakeRelativeMotionEvent.argtypes = [ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_ulong]
            pointer = xlib.XOpenDisplay(env['DISPLAY'].encode())
            assert pointer, 'could not open pointer-injection connection'
            try:
                # Enough movement to reach both virtual-stick stops, in small
                # events so a slow frame cannot hit the physical screen edge.
                for _ in range(100):
                    xtest.XTestFakeRelativeMotionEvent(pointer,10,8,0)
                    xlib.XFlush(pointer)
                    time.sleep(.01)
            finally:
                xlib.XCloseDisplay(pointer)
            frames(10)
            samples = rows('INPUT', start)
            assert samples and any(x or y for y,x,buttons,sx,sy in samples), 'injected mouse movement never reached flight'
            total = (sum(x for y,x,buttons,sx,sy in samples),sum(y for y,x,buttons,sx,sy in samples))
            assert total == (1000,800), f'relative mouse motion was lost or amplified: {total}'
            if args.flight_model != 'arcade':
                assert all(-512<=sx<=511 and -512<=sy<=511 for y,x,buttons,sx,sy in samples), 'long frame overran the virtual stick limits'
                assert samples[-1][3:] == (511,511), f'full diagonal stick did not settle at its bounds: {samples[-1]}'
            else:
                assert all(sx==sy==0 for y,x,buttons,sx,sy in samples), 'Arcade unexpectedly acquired a retained mouse stick'
            print(f'{args.flight_model}, presentation capped at {args.fps} fps: {len(samples)} updates; injected/consumed 1000,800; largest sample {max(abs(x) for y,x,b,sx,sy in samples)},{max(abs(y) for y,x,b,sx,sy in samples)}; stick range {min(sx for y,x,b,sx,sy in samples)}..{max(sx for y,x,b,sx,sy in samples)}, {min(sy for y,x,b,sx,sy in samples)}..{max(sy for y,x,b,sx,sy in samples)}')
    finally:
        if game and game.poll() is None:
            os.killpg(game.pid, signal.SIGTERM)
            try:
                game.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(game.pid, signal.SIGKILL)
                game.wait()
        display.terminate()
        display.wait(timeout=5)
