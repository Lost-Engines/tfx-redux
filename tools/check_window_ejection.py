#!/usr/bin/env python3
"""Exercise real double-Escape, authored seat release, parachute and FINISH.

Runs a private X11 window. GDB observes the pilot controller at launch and
parachute stages; it never changes game state, timing, input or outcomes.
"""
import argparse
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build',type=Path,default=Path('build'))
parser.add_argument('--data-dir',type=Path,required=True)
parser.add_argument('--mission',default='830')
parser.add_argument('--flight-model',choices=('arcade','simple','military-spec','realistic'),default='arcade')
a=parser.parse_args();executable=(a.build/'tfx').resolve();data=a.data_dir.resolve()
with tempfile.TemporaryDirectory(prefix='tfx-x11-ejection-') as temporary:
    directory=Path(temporary);install=directory/'TFX';install.mkdir()
    for name in ('DID.DAT','TFX.DAT'):(install/name).symlink_to(data/name)
    (install/'TFX.CFG').write_bytes(b'CONTROL=0\r\nMODEL=0\r\nSOUNDFX=0\r\nMUSIC=0\r\nREALISM=0,0,1,1,0,0,0\r\n')
    read,write=os.pipe();display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write);game=None
    try:
        if not select.select([read],[],[],5)[0]:raise RuntimeError('private X11 display did not start')
        number=os.read(read,64).decode().strip();os.close(read)
        env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
        # LeakSanitizer cannot inspect a process under GDB/ptrace; separate
        # sanitizer runs retain leak checking outside this observer test.
        env['ASAN_OPTIONS']=env.get('ASAN_OPTIONS','')+':detect_leaks=0'
        for name in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):env.pop(name,None)
        def run(*command):return subprocess.run(command,env=env,timeout=5,check=True,capture_output=True,text=True)
        def tap(key,settle=.4):
            run('xdotool','keydown',key);time.sleep(.12);run('xdotool','keyup',key);time.sleep(settle)
        debugger=directory/'observe.gdb'
        debugger.write_text('''set pagination off
set confirm off
set startup-with-shell off
set print thread-events off
break tfx::flight::ejection_sequence::advance_pilot if this->state.pilot.phase == tfx::flight::pilot_phase::launch
commands
silent
printf "TFX_TEST_PILOT_LAUNCH\\n"
disable 1
continue
end
break tfx::flight::ejection_sequence::advance_pilot if this->state.pilot.phase == tfx::flight::pilot_phase::parachute
commands
silent
printf "TFX_TEST_PILOT_PARACHUTE\\n"
disable 2
continue
end
run
''')
        if a.flight_model=='simple':
            observations=[]
            for number,(function,condition,label) in enumerate([
                ('tfx::flight::update_simple_controls','context.keys.down','DOWN'),
                ('tfx::flight::update_simple_controls','context.alt && context.keys.left','RUDDER'),
                ('tfx::flight::advance_simple','state.flight.arrestor_hook','HOOK'),
            ],start=3):
                observations.extend([f'break {function} if {condition}','commands','silent',f'printf "TFX_TEST_SIMPLE_{label}\\n"',f'disable {number}','continue','end'])
            debugger.write_text(debugger.read_text().replace('\nrun\n','\n'+'\n'.join(observations)+'\nrun\n'))
        if a.flight_model in ('military-spec','realistic'):
            observations=[]
            for number,(function,condition,label) in enumerate([
                ('tfx::flight::update_aerodynamic_controls','context.keys.down','DOWN'),
                ('tfx::flight::update_aerodynamic_controls','context.alt && context.keys.left','RUDDER'),
                ('tfx::flight::advance_aerodynamic_flight','state.arrestor_hook_down','HOOK'),
            ],start=3):
                observations.extend([f'break {function} if {condition}','commands','silent',f'printf "TFX_TEST_ADVANCED_{label}\\n"',f'disable {number}','continue','end'])
            debugger.write_text(debugger.read_text().replace('\nrun\n','\n'+'\n'.join(observations)+'\nrun\n'))
        with (directory/'game.log').open('w+') as log:
            def output():
                log.flush();return (directory/'game.log').read_text()
            def await_text(text,timeout=20):
                deadline=time.monotonic()+timeout
                while time.monotonic()<deadline and game.poll() is None:
                    if text in output():return
                    time.sleep(.1)
                raise RuntimeError('missing '+text+':\n'+output()[-5000:])
            game=subprocess.Popen(['gdb','--quiet','--batch','-x',str(debugger),'--args',str(executable),'--data-dir',str(install),'--mission',a.mission,'--flight-model',a.flight_model,'--control','keyboard','--scale','2'],env=env,stdout=log,stderr=subprocess.STDOUT)
            deadline=time.monotonic()+20;window=None
            while time.monotonic()<deadline and game.poll() is None:
                found=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True)
                if found.stdout.strip():window=found.stdout.splitlines()[0];break
                time.sleep(.05)
            if window is None:raise RuntimeError('game window did not appear:\n'+output()[-5000:])
            run('xdotool','windowfocus','--sync',window);time.sleep(.5)
            if a.flight_model=='simple':
                tap('Down');await_text('TFX_TEST_SIMPLE_DOWN')
                tap('Alt_L+Left');await_text('TFX_TEST_SIMPLE_RUDDER')
                tap('h');await_text('TFX_TEST_SIMPLE_HOOK')
                tap('h')
            if a.flight_model in ('military-spec','realistic'):
                run('xdotool','keydown','Down');await_text('TFX_TEST_ADVANCED_DOWN');run('xdotool','keyup','Down')
                run('xdotool','keydown','Alt_L+Left');await_text('TFX_TEST_ADVANCED_RUDDER');run('xdotool','keyup','Alt_L+Left')
                tap('h');await_text('TFX_TEST_ADVANCED_HOOK')
                tap('h')
            tap('Escape',1)
            assert 'TFX_TEST_PILOT_LAUNCH' not in output() and 'Mission result:' not in output(),'first Escape must not eject or end flight'
            tap('Escape')
            await_text('TFX_TEST_PILOT_LAUNCH')
            await_text('TFX_TEST_PILOT_PARACHUTE')
            tap('Escape')
            await_text('Mission result:')
            assert '; outcome 3' in output(),output()[-5000:]
            tap('Escape');game.wait(timeout=10)
            assert game.returncode==0 and 'exited normally' in output() and 'Program received signal' not in output(),output()[-5000:]
            if a.flight_model=='simple':print('Private X11 Simple controls passed: Down, Alt+Left and H reached their live flight consumers')
            print('Private X11 ejection passed: first press retained flight; authored SSD event launched pilot; parachute opened; Escape ran recovery/FINISH with outcome 3')
    finally:
        for process in (game,display):
            if process is not None and process.poll() is None:
                process.terminate()
                try:process.wait(timeout=3)
                except subprocess.TimeoutExpired:process.kill();process.wait()
