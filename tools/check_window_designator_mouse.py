#!/usr/bin/env python3
"""Exercise Z-view mouse routing on a private, silent X11 display.

GDB observes camera and flight input without altering game state. Run once with
Arcade and once with --flight-model military-spec to cover the retained mouse
stick. --redux also checks mouse aiming, wheel zoom and paused camera movement.
"""
import argparse
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
parser.add_argument('--flight-model', choices=('arcade', 'military-spec'), default='arcade')
parser.add_argument('--redux', action='store_true')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='tfx-designator-mouse-') as temporary:
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
continue
end
break tfx::cockpit::designator_pose
commands
silent
printf "AIM %u %u %u\n", state.pitch, state.heading, state.zoom
continue
end
break tfx::cockpit::designator_display::draw
commands
silent
printf "TRACK %d\n", tracking
continue
end
break tfx::cockpit::view_camera::advance
commands
silent
printf "VIEW %d\n", views.state.current
continue
end
run
''')
        command = [str((args.build / 'tfx').resolve()), '--data-dir', str(install), '--mission', '601', '--music', 'none', '--control', 'mouse', '--flight-model', args.flight_model, '--scale', '2']
        if args.redux:
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

            def tap(key):
                run('xdotool', 'keydown', key)
                frames(2)
                run('xdotool', 'keyup', key)
                frames(2)

            def motion(x, y):
                run('xdotool', 'mousemove_relative', '--', str(x), str(y))
                frames(3)

            def wheel(button):
                run('xdotool', 'click', str(button))
                frames(3)

            frames()
            tap('o')
            tap('z')
            wait(lambda: bool(rows('AIM')) and rows('TRACK')[-1] == (0,), 'Z did not enter ground search')
            frames(5)
            initial = rows('AIM')[-1]
            start = len(log.read_text())
            run('xdotool', 'mousedown', '1')
            motion(35, 15)
            run('xdotool', 'mouseup', '1', 'mousedown', '3')
            frames(4)
            run('xdotool', 'mouseup', '3')
            frames(4)
            controls = rows('INPUT', start)
            assert controls and all(not any(values) for values in controls), f'Z mouse reached flight controls: {controls}'
            moved = rows('AIM')[-1]
            if args.redux:
                yaw = (moved[1] - initial[1] + 32768) % 65536 - 32768
                assert moved[0] > initial[0] and yaw > 0, f'mouse right/down did not aim right/down: {initial} -> {moved}'
                wheel(4)
                assert rows('AIM')[-1][2] == moved[2] + 1, 'wheel up did not zoom in'
                wheel(5)
                assert rows('AIM')[-1][2] == moved[2], 'wheel down did not zoom out'
            else:
                assert moved == initial, 'native designator unexpectedly followed the mouse'

            tap('Pause')
            frames(4)
            ticks = len(rows('INPUT'))
            before = rows('AIM')[-1]
            motion(-20, -5)
            wheel(4)
            if args.redux:
                after = rows('AIM')[-1]
                assert after[:2] != before[:2] and after[2] == before[2] + 1, 'paused mouse/wheel did not update designator'
            assert len(rows('INPUT')) == ticks, 'mouse or wheel resumed flight'
            # The search/tracking indicator must stop blinking as well.
            from PIL import ImageGrab
            frames(6)
            image = ImageGrab.grab(xdisplay=env['DISPLAY']).tobytes()
            frames(8)
            assert image == ImageGrab.grab(xdisplay=env['DISPLAY']).tobytes(), 'paused designator image continued animating'
            tap('Return')
            wait(lambda: len(rows('INPUT')) > ticks, 'Enter did not resume flight')
            tap('z')
            wait(lambda: rows('TRACK')[-1] == (1,), 'Z did not designate a ground point')
            tap('Pause')
            frames(5)
            locked = rows('AIM')[-1]
            ticks = len(rows('INPUT'))
            motion(30, 20)
            assert rows('AIM')[-1][:2] == locked[:2] and rows('TRACK')[-1] == (1,), 'mouse lost or moved a locked designation'
            if args.redux:
                wheel(4)
                assert rows('AIM')[-1][2] == locked[2] + 1, 'locked target could not zoom while paused'
            assert len(rows('INPUT')) == ticks, 'locked camera manipulation resumed flight'
            tap('Return')
            tap('x')
            wait(lambda: rows('TRACK')[-1] == (0,), 'X did not release designation')
            tap('F1')
            wait(lambda: rows('VIEW')[-1] == (0,), 'F1 did not return to cockpit')
            frames(4)
            start = len(log.read_text())
            frames(8)
            controls = rows('INPUT', start)
            assert controls and all(not any(values) for values in controls), f'designator input leaked into cockpit: {controls}'
            motion(30, 15)
            assert any(any(values) for values in rows('INPUT', start)), 'mouse flight controls did not work after returning to cockpit'
            print(f'{args.flight_model}: Z mouse movement/buttons cannot steer or throttle; no retained input on exit; cockpit steering resumes. ' + ('Redux aiming, wheel zoom, paused rendering and locked tracking passed.' if args.redux else 'Native aiming unchanged; paused rendering remains still.'))
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
