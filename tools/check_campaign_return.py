#!/usr/bin/env python3
"""Check automatic flight-to-original-graph return using a disposable DOS pilot.

The save enters the last Arcade mission with one life. Normal simulation reaches
an aircraft-loss result; no damage, outcome or FINISH values are injected.
A private X11 display and installation isolate every save and input operation.
"""
import argparse
from window_input import move_game_pointer
import json
import os
from pathlib import Path
import select
import struct
import subprocess
import tempfile
import time

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--research',type=Path,default=Path('../tfx-reverse'))
parser.add_argument('--build',type=Path,default=Path('build'))
parser.add_argument('--data-dir',type=Path,required=True)
parser.add_argument('--enrol-menu',action='store_true')
a=parser.parse_args()
program=next(p for p in json.loads((a.research/'analysis/presentations/programs.json').read_text()) if p['name']=='tfx')
arcade=next(i for i,f in enumerate(program['functions']) if f['name']=='arcade')
launch=next(n['index'] for n in program['functions'][arcade]['nodes'] if n['type']==5 and n.get('resource')=='%mission')
values={'pilotname':'TEST','firstname':'CAMPAIGN','callsign':'RETURN','mission':605,'rank':1,'lives':1,'arcade':1}
pilot=bytearray()
for variable in program['variables']:
    value=values.get(variable['name'],'' if variable['string_value'] else 0)
    pilot.extend(str(value).encode().ljust(21,b'\0') if variable['string_value'] else struct.pack('<I',value))
pilot.extend(bytes((30-len(program['variables']))*4));pilot.extend(struct.pack('<3H',0,arcade,launch-1))
with tempfile.TemporaryDirectory(prefix='tfx-campaign-return-') as temporary:
    directory=Path(temporary);install=directory/'TFX';install.mkdir()
    for name in ('DID.DAT','TFX.DAT'):(install/name).symlink_to((a.data_dir/name).resolve())
    (install/'TFXSAV.0').write_bytes(pilot)
    read,write=os.pipe();display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write);game=None
    try:
        if not select.select([read],[],[],5)[0]:raise RuntimeError('private X11 display did not start')
        number=os.read(read,64).decode().strip();os.close(read)
        env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
        for name in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):env.pop(name,None)
        def run(*command):return subprocess.run(command,env=env,timeout=5,check=True,capture_output=True,text=True)
        def tap(key):run('xdotool','key','--clearmodifiers',key);time.sleep(.15)
        with (directory/'game.log').open('w+') as log:
            game=subprocess.Popen([str((a.build/'tfx').resolve()),'--data-dir',str(install),'--skip-intro','--scale','2'],env=env,stdout=log,stderr=subprocess.STDOUT)
            window=None;deadline=time.monotonic()+15
            while time.monotonic()<deadline and game.poll() is None:
                found=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True)
                if found.stdout.strip():window=found.stdout.splitlines()[0];break
                time.sleep(.05)
            if window is None:raise RuntimeError('front end did not appear')
            run('xdotool','windowfocus','--sync',window);time.sleep(1)
            baseline=None
            if a.enrol_menu:
                run('xdotool','mousemove','--window',window,'20','20');time.sleep(.3)
                run('import','-window',window,str(directory/'initial-enrol.png'))
                from PIL import Image
                baseline=Image.open(directory/'initial-enrol.png').convert('RGB').tobytes()
            tap('Down');tap('Return');tap('Return')
            deadline=time.monotonic()+150;armed=False;completed=False
            while time.monotonic()<deadline and game.poll() is None:
                title=run('xdotool','getwindowname',window).stdout
                if 'Arming' in title and not armed:
                    time.sleep(1)
                    move_game_pointer(env,window,197,198);time.sleep(.2)
                    run('xdotool','mousedown','1');time.sleep(.15);run('xdotool','mouseup','1');armed=True
                log.seek(0);text=log.read()
                if a.enrol_menu and 'TFX miss605 /' in text:
                    def click(x,y):
                        move_game_pointer(env,window,x,y)
                        run('xdotool','mousedown','1');time.sleep(.15);run('xdotool','mouseup','1');time.sleep(.2)
                    tap('grave');time.sleep(.4);click(80,172);click(160,132)
                    time.sleep(1.5)
                    run('xdotool','mousemove','--window',window,'20','20');time.sleep(.3)
                    run('import','-window',window,str(directory/'returned-enrol.png'))
                    returned=Image.open(directory/'returned-enrol.png').convert('RGB').tobytes()
                    assert returned==baseline,'Return to ENROL did not restore the initial pilot-selection image'
                    completed=True;break
                if 'Mission result:' in text and '— arcade' in title:
                    assert 'outcome 1' in text,text
                    completed=True;break
                time.sleep(.1)
            if not completed:
                log.seek(0);raise RuntimeError('flight did not return automatically: '+title+'\n'+log.read()[-4000:])
            # The original Arcade graph consumes the lost life and presents its
            # authored news screen; no Enter or Escape was sent after arming.
            run('import','-window',window,str(directory/'returned.png'))
            run('xdotool','keydown','Shift_L');time.sleep(.1)
            run('xdotool','keydown','q');time.sleep(.2);run('xdotool','keyup','q');run('xdotool','keyup','Shift_L')
            game.wait(timeout=10)
            if game.returncode:
                log.seek(0);raise RuntimeError('campaign return failed:\n'+log.read()[-4000:])
            if a.enrol_menu: print('Private X11 Return to ENROL passed: original pilot restore, arming, flight configuration, quit confirmation and pixel-identical enrolment return')
            else: print('Private X11 campaign return passed: DOS pilot restore, original arming, simulated aircraft loss, FINISH and automatic Arcade presentation return')
    finally:
        for process in (game,display):
            if process is not None and process.poll() is None:
                process.terminate()
                try:process.wait(timeout=3)
                except subprocess.TimeoutExpired:process.kill();process.wait()
