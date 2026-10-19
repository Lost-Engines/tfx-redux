#!/usr/bin/env python3
"""Exercise Shift+N and external-view palette handoffs on a private X11 display."""
import argparse
import json
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time
from window_input import confirm_flight_quit
from PIL import Image
from native_configuration import read_configuration

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--research',type=Path,default=Path('../tfx-reverse'))
parser.add_argument('--build',type=Path,default=Path('build'))
a=parser.parse_args();research=a.research.resolve();executable=(a.build/'tfx').resolve();data=research/'installs/cd_1994_09/TFX'
captures=json.loads((research/'analysis/lighting/environments.json').read_text())['environments']
def palette(capture,offset):
    bank=next(bank for bank in capture['banks'] if bank['data_offset']==offset)
    raw=(research/'extracted/cd_1994_09/resources'/bank['path']).read_bytes()
    return [tuple((value*255+31)//63 for value in raw[index:index+3]) for index in range(0,768,3)]
with tempfile.TemporaryDirectory(prefix='tfx-x11-palettes-') as temporary:
    directory=Path(temporary);read,write=os.pipe();display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write);game=None
    try:
        if not select.select([read],[],[],5)[0]:raise RuntimeError('private X11 display did not start')
        number=os.read(read,64).decode().strip();os.close(read)
        env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
        for name in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):env.pop(name,None)
        def run(*command):return subprocess.run(command,env=env,timeout=5,check=True,capture_output=True,text=True)
        def tap(key,settle=.3):
            run('xdotool','keydown',key);time.sleep(.1);run('xdotool','keyup',key);time.sleep(settle)
        for lighting in ('yugos_n','yugos_d'):
            capture=next(c for c in captures if c['environment']==lighting);normal_bank=palette(capture,'0x21000');normal=set(normal_bank);infrared=set(palette(capture,'0xea88'))
            with (directory/'game.log').open('w+') as log:
                game=subprocess.Popen([str(executable),'--data-dir',str(data),'--mission','501','--environment',lighting,'--control','keyboard','--flight-model','arcade','--scale','2'],env=env,stdout=log,stderr=subprocess.STDOUT)
                deadline=time.monotonic()+15;window=None
                while time.monotonic()<deadline and game.poll() is None:
                    found=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True)
                    if found.stdout.strip():window=found.stdout.splitlines()[0];break
                    time.sleep(.05)
                if window is None:
                    log.seek(0);raise RuntimeError('game window did not appear:\n'+log.read()[-4000:])
                run('xdotool','windowfocus','--sync',window);time.sleep(.4)
                def check(want_infrared,expected_green=None):
                    if game.poll() is not None:
                        log.seek(0);raise RuntimeError('flight terminated:\n'+log.read()[-4000:])
                    path=directory/'frame.png';run('import','-window',window,str(path))
                    with Image.open(path) as image:raw=image.convert('RGB').tobytes()
                    pixels=list(zip(raw[0::3],raw[1::3],raw[2::3]))
                    selected,other=(infrared,normal) if want_infrared else (normal,infrared)
                    outside=sum(pixel not in selected for pixel in pixels);distinct=sum(pixel in selected-other for pixel in pixels)
                    # Fixed cockpit colours and altitude bands alter the normal
                    # bank; infrared still uploads the complete authored bank.
                    if want_infrared:assert outside==0 and distinct>500,(lighting,want_infrared,outside,distinct)
                    else:assert sum(pixel not in infrared for pixel in pixels)>500,(lighting,want_infrared)
                    if expected_green is not None:
                        expected=(normal_bank[6][0],((expected_green&63)*255+31)//63,normal_bank[6][2])
                        assert pixels.count(expected)>10,(lighting,'HUD green',expected,pixels.count(expected))
                check(False,read_configuration(data)["hud_green"])
                run('xdotool','keydown','Shift_L');tap('n');run('xdotool','keyup','Shift_L');check(lighting.endswith('_n'))
                tap('F5',3);check(False)
                tap('F1',3);check(lighting.endswith('_n'))
                if lighting.endswith('_d'):
                    run('xdotool','keydown','u');time.sleep(2);run('xdotool','keyup','u');check(False,90)
                    run('xdotool','keydown','y');time.sleep(2);run('xdotool','keyup','y');check(False,127)
                tap('Shift_L+q');confirm_flight_quit(env,window);game.wait(timeout=10)
                if game.returncode:
                    log.seek(0);raise RuntimeError(log.read()[-4000:])
        print('Private X11 palette checks passed: Shift+N, day/night eligibility, external-view exclusion, cockpit restoration and Y/U HUD intensity clamps')
    finally:
        for process in (game,display):
            if process is not None and process.poll() is None:
                process.terminate()
                try:process.wait(timeout=3)
                except subprocess.TimeoutExpired:process.kill();process.wait()
