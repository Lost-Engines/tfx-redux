#!/usr/bin/env python3
"""Check Redux arming with real input on a private display and disposable install.

GDB observes weapon placement and inventory only; it never changes game state.
Requires Xvfb, xdotool and GDB. The arming probe supplies picks from the original
mount volumes, and checks all three aircraft before the window test begins.
"""
import argparse
import json
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time
from window_input import move_game_pointer, confirm_flight_quit

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=Path('build'))
parser.add_argument('--data-dir', type=Path, required=True)
parser.add_argument('--flight-key-hold', type=float, default=.25, help='seconds to hold polled flight keys; increase for instrumented builds')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='tfx-redux-arming-') as temporary:
    root = Path(temporary)
    install = root / 'TFX'
    install.mkdir()
    for name in ('DID.DAT', 'TFX.DAT'):
        (install / name).symlink_to((args.data_dir / name).resolve())
    config = b'CONTROL=0\r\nMODEL=2\r\nSOUNDFX=0\r\nMUSIC=0\r\nREALISM=0,0,1,1,0,0,0\r\n'
    (install / 'TFX.CFG').write_bytes(config)
    subprocess.run([str((args.build / 'redux_arming_probe').resolve()), str(install), str(root)], check=True)
    picks = json.loads((root / 'controls-1.json').read_text())
    read, write = os.pipe()
    display = subprocess.Popen(['Xvfb', '-displayfd', str(write), '-screen', '0', '1024x768x24', '-nolisten', 'tcp'], pass_fds=(write,), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.close(write)
    game = None
    try:
        if not select.select([read], [], [], 5)[0]:
            raise RuntimeError('private display did not start')
        number = os.read(read, 64).decode().strip()
        os.close(read)
        environment = dict(os.environ, DISPLAY=':' + number, LIBGL_ALWAYS_SOFTWARE='1', XDG_RUNTIME_DIR=temporary, XMODIFIERS='@im=none')
        for name in ('WAYLAND_DISPLAY', 'XDG_SESSION_TYPE', 'DBUS_SESSION_BUS_ADDRESS'):
            environment.pop(name, None)
        # LeakSanitizer cannot run under ptrace; separate non-GDB checks retain it.
        environment['ASAN_OPTIONS'] = environment.get('ASAN_OPTIONS', '') + ':detect_leaks=0'
        def run(*command):
            return subprocess.run(command, env=environment, capture_output=True, text=True, check=True, timeout=5)
        def tap(key, hold=.18):
            run('xdotool', 'keydown', key)
            time.sleep(hold)
            run('xdotool', 'keyup', key)
            time.sleep(hold)
        log = root / 'game.log'
        script = root / 'observe.gdb'
        script.write_text('''set pagination off
set confirm off
set startup-with-shell off
set print thread-events off
set $inventory = 0
break tfx::weapons::armament::select
commands
silent
set $inventory = this
continue
end
break tfx::flight::player_flight::advance
commands
silent
python
import json
if int(gdb.parse_and_eval('$inventory')):
    inventory=gdb.parse_and_eval('$inventory').dereference()['pylons']['_M_elems']
    print('REDUX_FRAME '+json.dumps([[int(inventory[i]['weapon']),int(inventory[i]['count']),bool(inventory[i]['visible'])]for i in range(11)]))
end
continue
end
break tfx::presentation::arming_controller::choose
commands
silent
printf "REDUX_CHOOSE %d\\n", weapon
continue
end
break tfx::presentation::arming_controller::place
commands
silent
printf "REDUX_PLACE %u\\n", slot
continue
end
run
''')
        with log.open('w') as stream:
            game = subprocess.Popen(['gdb', '--quiet', '--batch', '-x', str(script), '--args', str((args.build / 'tfx').resolve()), '--data-dir', str(install), '--skip-intro', '--resolution=800x500', '--scale=1'], env=environment, stdout=stream, stderr=subprocess.STDOUT)
            def output():
                return log.read_text()
            def wait(predicate, message, timeout=20):
                deadline = time.monotonic() + timeout
                while time.monotonic() < deadline and game.poll() is None:
                    if predicate():
                        return
                    time.sleep(.05)
                raise RuntimeError(message + '\n' + output()[-5000:])
            def windows():
                found = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', 'TFX Redux'], env=environment, capture_output=True, text=True, timeout=5)
                return found.stdout.splitlines()
            wait(windows, 'window did not open')
            window = windows()[0]
            run('xdotool', 'windowfocus', '--sync', window)
            time.sleep(1)
            def title():
                return run('xdotool', 'getwindowname', window).stdout.strip()
            def click(x, y, expanded=False, button=1):
                if expanded:
                    # The 800x500 target maps the 320x200 UI at 2.5x, while
                    # authored front-end screens use integer final scaling.
                    run('xdotool', 'mousemove', '--window', window, str(round((x + .5) * 2.5)), str(round((y + .5) * 2.5)))
                else:
                    move_game_pointer(environment, window, x, y)
                time.sleep(.15)
                run('xdotool', 'mousedown', str(button))
                time.sleep(.12)
                run('xdotool', 'mouseup', str(button))
                time.sleep(.15)
            tap('Return')
            run('xdotool', 'type', '--clearmodifiers', 'Ada')
            tap('Return')
            run('xdotool', 'type', '--clearmodifiers', 'Lovelace')
            tap('Return')
            tap('ctrl+Return')
            tap('Return')
            time.sleep(1.5)
            menu_title = title()
            def enter_fws():
                click(120, 76)
                deadline = time.monotonic() + 20
                while time.monotonic() < deadline and game.poll() is None:
                    if 'Arming' in title():
                        time.sleep(2)
                        return
                    tap('Escape')
                raise RuntimeError('FWS did not reach arming\n' + output()[-3000:])
            enter_fws()
            tap('Escape')
            wait(lambda: title() == menu_title, 'preflight Escape did not restore the previous menu')
            time.sleep(1)
            enter_fws()
            click(*picks['weapon'], expanded=True)
            click(*picks['mount'], expanded=True)
            wait(lambda: 'REDUX_PLACE ' in output(), 'real cursor click did not place the weapon')
            click(*picks['mount'], expanded=True, button=3)
            click(100, 75, expanded=True)
            run('xdotool', 'mousemove_relative', '--', '2000', '0')
            time.sleep(.2)
            location = dict(line.split('=', 1) for line in run('xdotool', 'getmouselocation', '--shell').stdout.splitlines() if '=' in line)
            geometry = dict(line.split('=', 1) for line in run('xdotool', 'getwindowgeometry', '--shell', window).stdout.splitlines() if '=' in line)
            assert int(geometry['X']) <= int(location['X']) < int(geometry['X']) + int(geometry['WIDTH']), 'rotation mouse escaped the window'
            assert int(geometry['Y']) <= int(location['Y']) < int(geometry['Y']) + int(geometry['HEIGHT']), 'rotation mouse escaped vertically'
            tap('Escape')
            click(*picks['done'], expanded=True)
            def frames():
                return [json.loads(line[len('REDUX_FRAME '):]) for line in output().splitlines() if line.startswith('REDUX_FRAME ')]
            wait(lambda: bool(frames()), 'armed mission did not enter flight')
            # The existing cheat gates in-flight rearming; no debugger writes.
            run('xdotool', 'keydown', 'Shift_R')
            for key in ('p', 'l', 'o', 'p'):
                tap(key, args.flight_key_hold)
            run('xdotool', 'keyup', 'Shift_R')
            before = frames()[-1]
            tap('ctrl+d', args.flight_key_hold)
            wait(lambda: 'Arming' in title(), 'developer rearming did not open')
            time.sleep(1)
            stopped = len(frames())
            click(*picks['defaults'], expanded=True)
            tap('Escape')
            time.sleep(.4)
            assert len(frames()) == stopped, 'Escape skipped the return confirmation'
            tap('n')
            time.sleep(.3)
            assert len(frames()) == stopped, 'No left the rearming menu'
            tap('Escape')
            tap('y')
            wait(lambda: len(frames()) > stopped, 'Yes did not return to flight')
            assert frames()[-1] == before, ('cancelled rearm changed inventory', before, frames()[-1])
            tap('shift+q', args.flight_key_hold)
            confirm_flight_quit(environment, window)
            game.wait(timeout=15)
            assert game.returncode == 0 and 'exited normally' in output(), output()[-5000:]
            assert 'Sanitizer' not in output() and 'ERROR:' not in output(), output()[-5000:]
            assert (install / 'TFX.CFG').read_bytes() == config, 'arming rewrote configuration'
            print('Private window passed: preflight back navigation, cursor pylon placement, paused rearm, Yes/No confirmation, inventory rollback and clean quit')
    finally:
        for process in (game, display):
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
