#!/usr/bin/env python3
"""Exercise live ILS engagement, HUD substitution, pause and camera return.

Expected heading legends use the original native font. Destination/controller
and complete-frame correctness have separate original-code comparisons.
"""
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
from native_reference import NativeReference
from native_configuration import read_configuration
from native_vga import VgaCapture

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build',type=Path,default=Path('build'))
parser.add_argument('--research',type=Path,default=Path('../tfx-reverse'))
parser.add_argument('--data-dir',type=Path,required=True)
parser.add_argument('--mission',default='909')
a=parser.parse_args();data=a.data_dir.resolve();executable=(a.build/'tfx').resolve();im=NativeReference(data/'TFX.DAT');im.cpu.mem_map(0x300000,4096);video=VgaCapture(im)
s=(a.research/'analysis/missions/data.js').read_text();missions=json.loads(s[s.index('=')+1:].strip().removesuffix(';'))['missions']
mission=next(m for m in missions if m['name']=='miss'+a.mission.removeprefix('miss'))
profiles=json.loads((a.research/'analysis/lighting/environments.json').read_text())['environments'];profile=next(p for p in profiles if p['environment']==mission['environment'])
bank=next(b for b in profile['banks'] if b['data_offset']=='0x21000');raw=(a.research/'extracted/cd_1994_09/resources'/bank['path']).read_bytes()
ink=tuple((value*255+31)//63 for value in (raw[18],read_configuration(data)["hud_green"]&63,raw[20]))
patterns={}
for label,edge in (('NAV',225),('AA',230),('AG',230)):
    im.cpu.mem_write(0x300000,label.encode()+b'\0');width=im.call(0x87114,eax=0x300000)&65535;video.reset();im.call(0x87154,eax=6,edx=edge-width,ebx=92,ecx=0x300000)
    patterns[label]=[(index%320,index//320) for index,value in enumerate(video.coverage()) if value]
landing_patterns=[]
for heading in range(360):
    label=f'TAKE HEADING {heading} FOR BEACON';im.cpu.mem_write(0x300000,label.encode()+b'\0')
    width=im.call(0x87114,eax=0x300000)&65535;video.reset();im.call(0x87154,eax=6,edx=160-width//2,ebx=92,ecx=0x300000)
    landing_patterns.append([(index%320,index//320) for index,value in enumerate(video.coverage()) if value])
with tempfile.TemporaryDirectory(prefix='tfx-x11-landing-') as temporary:
    directory=Path(temporary);read,write=os.pipe();display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write);game=None
    try:
        if not select.select([read],[],[],5)[0]:raise RuntimeError('private X11 display did not start')
        number=os.read(read,64).decode().strip();os.close(read)
        env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
        for name in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):env.pop(name,None)
        def run(*command):return subprocess.run(command,env=env,timeout=5,check=True,capture_output=True,text=True)
        def tap(key,settle=.4):
            run('xdotool','keydown',key);time.sleep(.1);run('xdotool','keyup',key);time.sleep(settle)
        with (directory/'game.log').open('w+') as log:
            game=subprocess.Popen([str(executable),'--data-dir',str(data),'--mission',a.mission,'--control','keyboard','--flight-model','arcade','--scale','2'],env=env,stdout=log,stderr=subprocess.STDOUT)
            deadline=time.monotonic()+15;window=None
            while time.monotonic()<deadline and game.poll() is None:
                found=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True)
                if found.stdout.strip():window=found.stdout.splitlines()[0];break
                time.sleep(.05)
            if window is None:
                log.seek(0);raise RuntimeError('game window did not appear:\n'+log.read()[-4000:])
            run('xdotool','windowfocus','--sync',window);time.sleep(.5)
            offset=None
            def check(label):
                global offset
                if game.poll() is not None:
                    log.seek(0);raise RuntimeError('flight terminated:\n'+log.read()[-4000:])
                path=directory/'frame.png';run('import','-window',window,str(path))
                with Image.open(path) as source:frame=source.convert('RGB').resize((320,200),Image.Resampling.NEAREST)
                def score(dy):return sum(frame.getpixel((x,y+dy))==ink for x,y in patterns[label])/len(patterns[label])
                if offset is None:offset=max(range(55),key=score)
                assert score(offset)==1,(label,offset,score(offset))
            def check_landing():
                path=directory/'landing.png';run('import','-window',window,str(path))
                with Image.open(path) as source:frame=source.convert('RGB').resize((320,200),Image.Resampling.NEAREST)
                assert any(all(frame.getpixel((x,y+offset))==ink for x,y in pattern) for pattern in landing_patterns),'ILS beacon heading legend is absent'
            check('NAV');tap('Return');check('AA')
            tap('i');check_landing()
            tap('p');tap('backslash');check_landing()  # Resume press is consumed; ILS remains engaged.
            tap('F5',2);tap('F1',3);check_landing()
            tap('backslash');check('NAV')
            tap('i');check_landing()
            tap('BackSpace');check('AG')
            tap('i');check('AG')
            tap('f');check('NAV')
            tap('backslash');check_landing()
            tap('i');check('NAV')
            tap('Shift_L+q');confirm_flight_quit(env,window);game.wait(timeout=10)
            if game.returncode:
                log.seek(0);raise RuntimeError(log.read()[-4000:])
            print('Private X11 ILS controls passed: I/backslash, beacon heading, NAV forcing, AA/AG handoffs, pause and camera restoration')
    finally:
        for process in (game,display):
            if process is not None and process.poll() is None:
                process.terminate()
                try:process.wait(timeout=3)
                except subprocess.TimeoutExpired:process.kill();process.wait()
