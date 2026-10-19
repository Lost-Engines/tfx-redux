#!/usr/bin/env python3
"""Exercise native camera keys and canopy switching on a private X11 display."""
import argparse
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time
from window_input import confirm_flight_quit
from PIL import Image

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build',type=Path,default=Path('build'))
parser.add_argument('--data-dir',type=Path,required=True)
parser.add_argument('--save-directory',type=Path)
parser.add_argument('--mission',default='501')
parser.add_argument('--environment',help='Override weather for camera/paused-frame checks')
a=parser.parse_args();executable=(a.build/'tfx').resolve();data=a.data_dir.resolve()
with tempfile.TemporaryDirectory(prefix='tfx-x11-views-') as temporary:
    directory=Path(temporary);captures=a.save_directory or directory
    captures.mkdir(parents=True,exist_ok=True)
    read,write=os.pipe();display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write);game=None
    try:
        if not select.select([read],[],[],5)[0]:raise RuntimeError('private X11 display did not start')
        number=os.read(read,64).decode().strip();os.close(read)
        env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
        for name in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):env.pop(name,None)
        def run(*command):return subprocess.run(command,env=env,timeout=5,check=True,capture_output=True,text=True)
        def tap(key,settle=.25):
            run('xdotool','keydown',key);time.sleep(.1);run('xdotool','keyup',key);time.sleep(settle)
        with (directory/'game.log').open('w+') as log:
            game=subprocess.Popen([str(executable),'--data-dir',str(data),'--mission',a.mission,'--scale','2']+(['--environment',a.environment] if a.environment else []),env=env,stdout=log,stderr=subprocess.STDOUT)
            deadline=time.monotonic()+15;window=None
            while time.monotonic()<deadline and game.poll() is None:
                found=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True)
                if found.stdout.strip():window=found.stdout.splitlines()[0];break
                time.sleep(.05)
            if window is None:
                log.seek(0);raise RuntimeError('game window did not appear:\n'+log.read()[-4000:])
            run('xdotool','windowfocus','--sync',window);time.sleep(.4)
            def image(name):
                if game.poll() is not None:
                    log.seek(0);raise RuntimeError('flight terminated:\n'+log.read()[-4000:])
                path=captures/(name+'.png');run('import','-window',window,str(path))
                with Image.open(path) as source:return source.convert('RGB').resize((320,200),Image.Resampling.NEAREST)
            initial=image('01-cockpit');footer=initial.crop((270,187,288,199)).tobytes()
            tap('F5',3);chase=image('02-chase')
            if chase.crop((270,187,288,199)).tobytes()==footer:raise RuntimeError('F5 retained the flat cockpit footer')
            tap('F1',3);front=image('03-return')
            if front.crop((270,187,288,199)).tobytes()!=footer:raise RuntimeError('F1 did not restore the original cockpit position')
            run('xdotool','keydown','Shift_L');tap('F1',.5);run('xdotool','keyup','Shift_L');full=image('04-full-forward')
            if full.crop((270,187,288,199)).tobytes()==footer:raise RuntimeError('Shift+F1 retained the instrument panel')
            tap('F1',1);image('05-cockpit-restored')
            for key,name in [('F2','06-left'),('F3','07-right'),('F4','08-rear'),('F7','09-tactical'),('F9','10-target'),('F10','11-virtual')]:
                tap(key,1.5);image(name)
                tap('F1',3)
            tap('F10',2);tap('p',.2);before=image('12-paused');time.sleep(.3);after=image('13-paused')
            if before.tobytes()!=after.tobytes():raise RuntimeError('paused camera continued moving')
            tap('p');tap('F1',3);tap('Shift_L+q');confirm_flight_quit(env,window);game.wait(timeout=10)
            if game.returncode:
                log.seek(0);raise RuntimeError(log.read()[-4000:])
            print('Private X11 camera controls passed: chase, full forward, head turns, rear, tactical, target, virtual cockpit, offset restoration and paused-frame retention')
    finally:
        for process in (game,display):
            if process is not None and process.poll() is None:
                process.terminate()
                try:process.wait(timeout=3)
                except subprocess.TimeoutExpired:process.kill();process.wait()
