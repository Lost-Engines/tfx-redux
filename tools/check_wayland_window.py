#!/usr/bin/env python3
"""Check first-frame startup with a private headless Wayland compositor.

Requires Sway and a developer build; never connects to the desktop session.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=Path('build'))
parser.add_argument('--data-dir', type=Path, required=True)
args = parser.parse_args()
sway = shutil.which('sway')
if not sway:
    parser.error('this optional desktop check requires Sway')
swaymsg = shutil.which('swaymsg')
if not swaymsg:
    parser.error('this optional desktop check requires swaymsg')
executable = str((args.build / 'tfx').resolve())
data_directory = str(args.data_dir.resolve())

with tempfile.TemporaryDirectory(prefix='tfx-wayland-test-') as directory:
    runtime = Path(directory)
    config = runtime / 'sway.conf'
    config.write_text('output * mode 1280x800\nfor_window [title="^TFX "] floating enable\n')
    environment = os.environ.copy()
    for name in ['DISPLAY', 'WAYLAND_DISPLAY', 'SWAYSOCK']:
        environment.pop(name, None)
    environment.update(
        XDG_RUNTIME_DIR=directory,
        XDG_SESSION_TYPE='wayland',
        WLR_BACKENDS='headless',
        WLR_HEADLESS_OUTPUTS='1',
        WLR_RENDERER='pixman',
        WLR_LIBINPUT_NO_DEVICES='1',
        LIBGL_ALWAYS_SOFTWARE='1',
    )
    with (runtime / 'sway.log').open('w+') as log:
        compositor = subprocess.Popen(
            [sway, '--unsupported-gpu', '--config', str(config)],
            env=environment, stdout=log, stderr=subprocess.STDOUT,
        )
        try:
            deadline = time.monotonic() + 5
            socket = None
            ipc = None
            while time.monotonic() < deadline and compositor.poll() is None:
                socket = next((p for p in runtime.glob('wayland-*') if p.is_socket()), None)
                ipc = next(iter(runtime.glob('sway-ipc.*.sock')), None)
                if socket and ipc:
                    break
                time.sleep(.05)
            if socket is None or ipc is None:
                log.seek(0)
                raise RuntimeError('headless Wayland startup failed:\n' + log.read()[-4000:])
            environment['WAYLAND_DISPLAY'] = socket.name
            def find_window(node):
                if (node.get('name') or '').startswith(('TFX C++','TFX Redux')):
                    return node
                for child in node.get('nodes', []) + node.get('floating_nodes', []):
                    found = find_window(child)
                    if found:
                        return found
                return None

            for scale in [0, 2]:
                app = subprocess.Popen(
                    [executable, '--data-dir', data_directory, '--scale', str(scale)],
                    env=environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                )
                try:
                    deadline = time.monotonic() + 5
                    window = None
                    while time.monotonic() < deadline and app.poll() is None:
                        result = subprocess.check_output(
                            [swaymsg, '--socket', str(ipc), '--type', 'get_tree'], env=environment,
                        )
                        window = find_window(json.loads(result))
                        if window and window.get('visible'):
                            break
                        time.sleep(.05)
                    if window is None or not window.get('visible'):
                        raise RuntimeError(f'no visible Wayland window at scale={scale}')
                    # Headless Sway has no keyboard: this must map even while
                    # the application is waiting for initial keyboard focus.
                    subprocess.run(
                        [swaymsg, '--socket', str(ipc), f'[con_id={window["id"]}] kill'],
                        env=environment, check=True, capture_output=True,
                    )
                    stdout, stderr = app.communicate(timeout=5)
                    if app.returncode:
                        raise RuntimeError(stdout + stderr)
                    print(f'Wayland startup passed: scale={scale}, visible first frame without keyboard focus, clean window close')
                finally:
                    if app.poll() is None:
                        app.terminate()
                        try:
                            app.communicate(timeout=3)
                        except subprocess.TimeoutExpired:
                            app.kill()
                            app.communicate()
        finally:
            compositor.terminate()
            try:
                compositor.wait(timeout=3)
            except subprocess.TimeoutExpired:
                compositor.kill()
                compositor.wait()
