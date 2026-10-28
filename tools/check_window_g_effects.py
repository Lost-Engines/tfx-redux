#!/usr/bin/env python3
"""Check live G effects in a disposable installation and silent audio sink.

GDB supplies G-load at the controller input, without altering flight physics or
its outputs. The real strain controller, palette, HUD, sample scheduler and PCM
output run normally. Native arithmetic has separate Unicorn comparisons.
Requires GDB with Python, Xvfb, xdotool, ImageMagick, Pillow and PulseAudio tools.
"""
import argparse
import array
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import tempfile
import time
from PIL import Image
from window_input import confirm_flight_quit


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=Path('build'))
    parser.add_argument('--data-dir', type=Path, required=True)
    parser.add_argument('--redux', action='store_true', help='check the virtual cockpit with vector HUD')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='tfx-g-effects-') as temporary:
        directory = Path(temporary)
        install = directory / 'TFX'
        install.mkdir()
        for name in ('DID.DAT', 'TFX.DAT'):
            (install / name).symlink_to((args.data_dir / name).resolve())
        (install / 'TFX.CFG').write_bytes(
            b'CONTROL=0\r\nMODEL=3\r\nSOUNDFX=1\r\nMUSIC=1\r\n'
            b'REALISM=2,0,1,1,0,0,0\r\nDETAIL=2\r\n')
        control = directory / 'load.json'
        observations = directory / 'observations.jsonl'
        pcm = directory / 'capture.f32'
        log = directory / 'game.log'
        sink = 'tfx_g_effects_' + str(os.getpid())
        module = display = game = capture = None

        def set_load(value):
            pending = directory / 'pending.json'
            pending.write_text(json.dumps({'load': value}))
            pending.replace(control)

        set_load(1)
        # Observe downstream requests as well as real window/PCM output. Only
        # the supplied load is mutated, inside this private child process.
        debugger = directory / 'observe.gdb'
        debugger.write_text("""set pagination off
set confirm off
set startup-with-shell off
set print thread-events off
python
import gdb, json
from pathlib import Path
control = Path(CONTROL_PATH)
observations = Path(OBSERVATIONS_PATH)
def record(event):
    with observations.open('a') as output:
        output.write(json.dumps(event) + '\\n')
class Load(gdb.Breakpoint):
    def stop(self):
        value = json.loads(control.read_text())['load']
        gdb.execute('set variable load = ' + str(float(value)), to_string=True)
        return False
class Palette(gdb.Breakpoint):
    previous = None
    def stop(self):
        context = gdb.parse_and_eval('context')
        if self.previous is None:
            record(dict(kind='process', pid=gdb.selected_inferior().pid))
        current = tuple(int(context[field]) for field in ('blackout', 'blackout_level', 'redout_level'))
        if current != self.previous:
            record(dict(kind='palette', mode=current[0], black=current[1], red=current[2]))
            self.previous = current
        return False
class Sound(gdb.Breakpoint):
    def stop(self):
        sample = int(gdb.parse_and_eval('id'))
        if sample in (65, 66, 67):
            record(dict(kind='breath', sample=sample, priority=int(gdb.parse_and_eval('priority'))))
        return False
Load('tfx::render::advance_g_effects')
Palette('tfx::render::palette_lighting::advance')
Sound('tfx::audio::runtime::sample(tfx::audio::sample_priority, unsigned int)')
end
run
""".replace('CONTROL_PATH', repr(str(control)))
            .replace('OBSERVATIONS_PATH', repr(str(observations))))
        try:
            module = subprocess.check_output([
                'pactl', 'load-module', 'module-null-sink', 'sink_name=' + sink,
                'rate=48000', 'channels=2'], text=True).strip()
            read, write = os.pipe()
            display = subprocess.Popen([
                'Xvfb', '-displayfd', str(write), '-screen', '0', '1024x768x24', '-nolisten', 'tcp'],
                pass_fds=(write,), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            os.close(write)
            try:
                if not select.select([read], [], [], 5)[0]:
                    raise RuntimeError('private display did not start')
                number = os.read(read, 64).decode().strip()
            finally:
                os.close(read)
            env = dict(os.environ, DISPLAY=':' + number, LIBGL_ALWAYS_SOFTWARE='1',
                       XMODIFIERS='@im=none', PULSE_SINK=sink, XDG_SESSION_TYPE='x11')
            for key in ('WAYLAND_DISPLAY', 'DBUS_SESSION_BUS_ADDRESS'):
                env.pop(key, None)
            env['ASAN_OPTIONS'] = env.get('ASAN_OPTIONS', '') + ':detect_leaks=0'

            def run(*command):
                return subprocess.run(command, env=env, check=True, capture_output=True, text=True, timeout=10)

            def events():
                if not observations.exists():
                    return []
                return [json.loads(line) for line in observations.read_text().splitlines() if line.endswith('}')]

            def await_condition(condition, description, timeout=20):
                deadline = time.monotonic() + timeout
                while time.monotonic() < deadline and game.poll() is None:
                    if condition():
                        return
                    time.sleep(.05)
                raise RuntimeError(description + '\n' + log.read_text()[-6000:])

            with pcm.open('wb') as recording, log.open('w') as output:
                capture = subprocess.Popen([
                    'parec', '--device=' + sink + '.monitor', '--format=float32le',
                    '--rate=48000', '--channels=2', '--latency-msec=50'], stdout=recording, stderr=subprocess.DEVNULL, env=env)
                command = [str((args.build / 'tfx').resolve()), '--data-dir', str(install),
                           '--mission', '513', '--music', 'none', '--qsound', 'off', '--scale', '2']
                if args.redux:
                    command += ['--resolution', '960x600', '--fov', '100']
                game = subprocess.Popen(['gdb', '--quiet', '--batch', '-x', str(debugger), '--args', *command],
                                        env=env, stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
                await_condition(lambda: events() and 'PCM output:' in log.read_text(), 'flight/audio did not start', 45)
                window = run('xdotool', 'search', '--onlyvisible', '--name', 'TFX').stdout.splitlines()[0]
                run('xdotool', 'windowfocus', '--sync', window)
                pid = next(event['pid'] for event in events() if event['kind'] == 'process')
                streams = json.loads(run('pactl', '-f', 'json', 'list', 'sink-inputs').stdout)
                owned = [stream for stream in streams if stream.get('properties', {}).get('application.process.id') == str(pid)]
                assert len(owned) == 1, 'cannot identify the private game audio stream'
                run('pactl', 'move-sink-input', str(owned[0]['index']), sink)

                def tap(key):
                    run('xdotool', 'keydown', key)
                    time.sleep(.12)
                    run('xdotool', 'keyup', key)
                    time.sleep(.15)

                def picture():
                    filename = directory / 'frame.png'
                    run('import', '-window', window, str(filename))
                    with Image.open(filename) as image:
                        return image.convert('RGB').getextrema()

                def breaths():
                    return [event for event in events() if event['kind'] == 'breath']

                def mode():
                    return [event for event in events() if event['kind'] == 'palette'][-1]['mode']

                def peak(start):
                    data = pcm.read_bytes()[start:]
                    samples = array.array('f')
                    samples.frombytes(data[:len(data) // 8 * 8])
                    assert samples, 'no captured audio'
                    return max(map(abs, samples))

                time.sleep(2)
                assert any(high > 0 for low, high in picture()), 'normal view is blank'
                assert not breaths(), 'breathing at the supplied 1 G'
                for load, expected, target in ((15, 2, ((0, 0),) * 3), (-8, 1, ((255, 255), (0, 0), (0, 0)))):
                    start = pcm.stat().st_size
                    before = len(breaths())
                    set_load(load)
                    await_condition(lambda: mode() == expected and len(breaths()) > before, 'G effect did not reach audio')
                    time.sleep(2)
                    assert picture() == target, ('wrong full-strain pixels', load, picture())
                    assert peak(start) > .05, 'breath request did not reach the PCM device'
                    tap('Pause')
                    time.sleep(4)  # Drain the breath and the host output/capture buffers.
                    count = len(breaths())
                    start = pcm.stat().st_size
                    time.sleep(.5)
                    assert peak(start) < 1e-7, ('breathing continued while paused', peak(start))
                    tap('F2')  # The normal key click is allowed; another breath is not.
                    time.sleep(1)
                    assert len(breaths()) == count, 'paused repaint retriggered breathing'
                    assert picture() == target, 'paused camera escaped the G tint'
                    set_load(1)
                    tap('Pause')
                    await_condition(lambda: mode() == 0, 'G strain did not recover')
                    time.sleep(.5)
                    assert all(high > 0 for low, high in picture()), 'recovery left the screen black or red'
                    tap('F10' if args.redux else 'F1')
                assert {event['sample'] for event in breaths()} == {65, 66, 67}, 'missing breathing intensity'
                assert all(event['priority'] == 2 for event in breaths()), 'breathing lost native urgent priority'
                tap('Shift_L+q')
                confirm_flight_quit(env, window)
                game.wait(timeout=15)
                assert game.returncode == 0 and 'exited normally' in log.read_text(), log.read_text()[-6000:]
                assert 'ERROR:' not in log.read_text() and 'Traceback' not in log.read_text(), log.read_text()[-6000:]
                print('Live G effects pass: blackout/redout pixels, all three urgent breaths, real PCM, '
                      'recovery, paused camera redraw and clean shutdown' + (' (Redux vector HUD)' if args.redux else ''))
        finally:
            if game is not None and game.poll() is None:
                os.killpg(game.pid, signal.SIGTERM)
                try:
                    game.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    os.killpg(game.pid, signal.SIGKILL)
                    game.wait()
            for process in (capture, display):
                if process is not None and process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
            if module is not None:
                subprocess.run(['pactl', 'unload-module', module], check=True)


if __name__ == '__main__':
    main()
