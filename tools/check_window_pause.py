#!/usr/bin/env python3
"""Check pause/resume and held-fire suppression through private X11 input.

GDB observes simulation ticks and projectile launches without changing state.
The real install is read only; a disposable configuration selects silent Arcade.
"""
import argparse
import os
import re
from pathlib import Path
import select
import signal
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=Path('build'))
parser.add_argument('--data-dir', type=Path, default=Path('../tfx-reverse/installs/cd_1994_09/TFX'))
parser.add_argument('--redux', action='store_true')
parser.add_argument('--screenshots', type=Path)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='tfx-window-pause-') as temporary:
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
        script.write_text('''set pagination off
set confirm off
set startup-with-shell off
set print thread-events off
break tfx::flight::ejection_sequence::advance_aircraft
commands
silent
printf "TICK\\n"
continue
end
break tfx::weapons::runtime::launch
commands
silent
printf "FIRE\\n"
continue
end
break tfx::platform::window::present
commands
silent
printf "FRAME\\n"
continue
end
run
''')
        script.write_text(script.read_text().replace('run\n', r'''break tfx::cockpit::view_camera::advance
commands
silent
printf "VIEW %d\n", views.state.current
continue
end
break tfx::world::scene::advance_instance
commands
silent
printf "ANIMATION\n"
continue
end
run
'''))
        if args.redux:
            script.write_text(script.read_text().replace('run\n', r'''break tfx::cockpit::mouse_orbit::apply
commands
silent
printf "ORBIT %d %f %f\n", this->selected, this->heading, this->pitch
continue
end
run
'''))
        command = [str((args.build / 'tfx').resolve()), '--data-dir', str(install), '--mission', '501', '--music', 'none', '--scale', '2']
        if args.redux:
            command += ['--resolution', '640x400', '--scale', '1']
        with log.open('w') as output:
            game = subprocess.Popen(['gdb', '--batch', '-x', str(script), '--args', *command], env=env, cwd=root, stdout=output, stderr=subprocess.STDOUT, start_new_session=True)

            def run(*command):
                return subprocess.run(command, env=env, capture_output=True, text=True, check=True, timeout=5)

            def count(event):
                return log.read_text().splitlines().count(event)

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
                return bool(windows) and count('TICK') > 3

            wait(ready, 'flight did not start')
            run('xdotool', 'windowfocus', '--sync', windows[0])
            time.sleep(.2)

            def frames(number=3):
                start = count('FRAME')
                wait(lambda: count('FRAME') >= start + number, 'window stopped presenting')

            def tap(key):
                run('xdotool', 'keydown', key)
                frames(2)
                run('xdotool', 'keyup', key)
                frames(2)

            def paused():
                frames()
                ticks = count('TICK')
                frames(5)
                assert count('TICK') == ticks, 'simulation advanced while paused'

            def running():
                ticks = count('TICK')
                wait(lambda: count('TICK') > ticks + 2, 'resume key did not restart simulation')

            tap('Pause')
            paused()
            tap('Shift_L')
            paused()

            def picture(name):
                from PIL import ImageGrab
                dims = dict(line.split('=', 1) for line in run('xdotool', 'getwindowgeometry', '--shell', windows[0]).stdout.splitlines() if '=' in line)
                x, y, w, h = (int(dims[k]) for k in ('X', 'Y', 'WIDTH', 'HEIGHT'))
                image = ImageGrab.grab(xdisplay=env['DISPLAY']).crop((x, y, x+w, y+h))
                if args.screenshots:
                    args.screenshots.mkdir(parents=True, exist_ok=True)
                    image.save(args.screenshots / (name + '.png'))
                return image.tobytes()

            frozen_ticks, frozen_animation = count('TICK'), count('ANIMATION')
            front = picture('paused-front')
            tap('F4')
            wait(lambda: re.findall(r'^VIEW (\d+)$', log.read_text(), re.M)[-1] == '26', 'F4 did not select rear view while paused')
            frames(8)
            rear = picture('paused-rear')
            assert front != rear, 'camera changed internally but the paused image stayed cached'
            assert count('TICK') == frozen_ticks and count('ANIMATION') == frozen_animation, 'view switching advanced the world'
            tap('F5')
            wait(lambda: re.findall(r'^VIEW (\d+)$', log.read_text(), re.M)[-1] == '4', 'F5 did not complete its paused camera transition')
            frames(80)
            chase = picture('paused-chase')
            frames(8)
            assert chase == picture('paused-chase-still'), 'paused scene continued animating after the camera settled'
            if args.redux:
                run('xdotool', 'mousemove_relative', '--', '90', '30')
                frames(6)
                states = [(int(m), float(h), float(p)) for m,h,p in re.findall(r'^ORBIT (\d+) ([-.0-9]+) ([-.0-9]+)$', log.read_text(), re.M)]
                assert any(m == 4 and abs(h) > .05 and abs(p) > .05 for m,h,p in states), 'paused mouse did not orbit the aircraft'
                assert chase != picture('paused-orbit'), 'mouse orbit did not change the paused image'
            tap('4')
            wait(lambda: bool(list(install.glob('GRAB_*.LBM'))), 'screenshot key did not save while paused')
            assert count('TICK') == frozen_ticks and count('ANIMATION') == frozen_animation, 'mouse or screenshot input advanced the world'
            tap('F1')
            wait(lambda: re.findall(r'^VIEW (\d+)$', log.read_text(), re.M)[-1] == '0', 'F1 did not return to the cockpit while paused')
            paused()
            tap('Return')
            running()
            tap('p')
            paused()
            shots = count('FIRE')
            run('xdotool', 'keydown', 'space')
            running()
            frames(12)
            assert count('FIRE') == shots, 'held Space resume press also fired a weapon'
            run('xdotool', 'keyup', 'space')
            frames(3)
            tap('space')
            wait(lambda: count('FIRE') > shots, 'fresh Space press did not fire after releasing resume press')
            tap('Pause')
            paused()
            tap('g')
            running()
            tap('p')
            paused()
            tap('Pause')
            running()
            print('Paused F-key cameras, frozen scene, mouse orbit where supported and screenshot save passed. Live pause: Pause/P stop simulation; modifiers leave it paused; Enter, Space and G resume; held Space cannot fire until released and pressed again.')
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
