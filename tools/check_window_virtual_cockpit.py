#!/usr/bin/env python3
"""Check live virtual-cockpit screens on a private, silent X11 display.

Cycles both MFDs, including scene pages, and checks paused cockpit images and
view switching. GDB only observes frame delivery and input; it never changes
simulation state. Requires Xvfb, GDB, xdotool and Pillow.
"""
import argparse
import os
from pathlib import Path
import select
import signal
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=Path('build'))
parser.add_argument('--data-dir', type=Path, default=Path('../tfx-reverse/installs/cd_1994_09/TFX'))
parser.add_argument('--flight-model', choices=('arcade', 'military-spec'), default='arcade')
parser.add_argument('--mission', default='703')
parser.add_argument('--captures', type=Path, default=Path('/tmp/tfx-virtual-live'))
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='tfx-virtual-cockpit-') as temporary:
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
break tfx::cockpit::hud::draw
commands
silent
printf "MFD %u %u %u %u\n", state.instruments.aircraft, selection.left, selection.centre, selection.right
continue
end
break tfx::cockpit::virtual_displays::update
commands
silent
printf "LIVE\n"
continue
end
run
''')
        args.captures=args.captures.resolve()
        args.captures.mkdir(parents=True,exist_ok=True)
        command = [str((args.build / 'tfx').resolve()), '--data-dir', str(install), '--mission', args.mission, '--music', 'none', '--control', 'mouse', '--flight-model', args.flight_model, '--scale', '2']
        command += ['--resolution', '960x600', '--scale', '1']
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

            from PIL import ImageGrab
            frames(4)
            tap('F10')
            wait(lambda: 'LIVE' in log.read_text(), 'virtual displays were never rendered')
            run('xdotool','keydown','Shift_R','keydown','Down')
            frames(12)
            run('xdotool','keyup','Down','keyup','Shift_R')
            frames(12)
            ImageGrab.grab(xdisplay=env['DISPLAY']).save(args.captures/'initial.png')
            seen_left, seen_centre, seen_right=set(),set(),set()
            for page in range(9):
                tap('1')
                tap('3')
                if page%3==0: tap('2')
                frames(3)
                _,left,centre,right=rows('MFD')[-1]
                seen_left.add(left%9);seen_centre.add(centre%3);seen_right.add(right%9)
                ImageGrab.grab(xdisplay=env['DISPLAY']).save(args.captures/f'page-{page}.png')
            # Leave two 3D scene pages active during pause; neither the ground
            # target's orbit nor the camera reticle may continue animating.
            assert len(seen_left)>=8 and len(seen_right)>=8 and len(seen_centre)>=2, 'MFD page controls did not cycle'
            for column,key,page in ((1,'1',7),(3,'3',6)):
                for _ in range(10):
                    if rows('MFD')[-1][column]%9==page: break
                    tap(key)
                else: raise AssertionError('Scene page unavailable; choose a mission with ground targets')
            tap('Pause')
            frames(8)
            ticks=len(rows('INPUT'))
            frozen=ImageGrab.grab(xdisplay=env['DISPLAY']).tobytes()
            frames(12)
            assert frozen==ImageGrab.grab(xdisplay=env['DISPLAY']).tobytes(), 'paused cockpit kept animating'
            assert len(rows('INPUT'))==ticks, 'pause continued advancing flight'
            tap('F5')
            frames(12)
            tap('F1')
            frames(12)
            tap('F10')
            frames(12)
            ImageGrab.grab(xdisplay=env['DISPLAY']).save(args.captures/'paused.png')
            print(f'Live virtual cockpit (aircraft {rows("MFD")[-1][0]}): outer pages {sorted(seen_left)}/{sorted(seen_right)}, centre pages {sorted(seen_centre)}, scene cameras and paused switching passed')
    finally:
        if game and game.poll() is None:
            os.killpg(game.pid,signal.SIGTERM)
            try: game.wait(timeout=5)
            except subprocess.TimeoutExpired: os.killpg(game.pid,signal.SIGKILL)
        display.terminate()
        display.wait(timeout=5)
