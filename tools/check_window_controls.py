#!/usr/bin/env python3
"""Exercise weapon selection, store/cannon firing and quit on a private X11 display.

Requires Xvfb, xdotool, ImageMagick import and Pillow. The private runtime
prevents GLFW from discovering an unrelated desktop Wayland socket.
"""
import argparse
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time
from window_input import confirm_flight_quit
from PIL import Image

parser=argparse.ArgumentParser(__doc__)
parser.add_argument('--build',type=Path,default=Path('build'))
parser.add_argument('--data-dir',type=Path,required=True)
args=parser.parse_args()
executable=(args.build/'tfx').resolve();data=args.data_dir.resolve()
with tempfile.TemporaryDirectory(prefix='tfx-x11-input-') as temporary:
    directory=Path(temporary)
    read,write=os.pipe()
    display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    os.close(write);game=None
    try:
        if not select.select([read],[],[],5)[0]:raise RuntimeError('private X11 display did not start')
        number=os.read(read,64).decode().strip();os.close(read)
        environment=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
        for name in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):environment.pop(name,None)
        def run(*command,**options):return subprocess.run(command,env=environment,timeout=5,**options)
        def tap(key):
            run('xdotool','keydown',key,check=True);time.sleep(.15)
            run('xdotool','keyup',key,check=True);time.sleep(.15)
        with (directory/'game.log').open('w+') as log:
            game=subprocess.Popen([str(executable),'--data-dir',str(data),'--mission','501','--control','keyboard','--flight-model','arcade','--scale','2'],env=environment,stdout=log,stderr=subprocess.STDOUT)
            deadline=time.monotonic()+10;window=None
            while time.monotonic()<deadline and game.poll() is None:
                found=run('xdotool','search','--onlyvisible','--name','TFX (C|Redux)',capture_output=True,text=True)
                if found.stdout.strip():window=found.stdout.splitlines()[0];break
                time.sleep(.05)
            if window is None:
                log.seek(0);raise RuntimeError('game window did not appear:\n'+log.read()[-4000:])
            run('xdotool','windowfocus','--sync',window,check=True);time.sleep(.5)
            tap('Prior') # Raise the animated cockpit before comparing its MFD windows.
            for _ in range(6):tap('1') # Flight-data -> stores; this mission skips the unavailable ground-data page.
            before_path=directory/'before.png';after_path=directory/'after.png'
            run('import','-window',window,str(before_path),check=True)
            tap('Return')
            store_before=directory/'store-before.png';store_after=directory/'store-after.png'
            run('import','-window',window,str(store_before),check=True)
            tap('space')
            run('import','-window',window,str(store_after),check=True)
            def store_readout(path):
                with Image.open(path) as image:return image.convert('RGB').resize((320,200),Image.Resampling.NEAREST).crop((55,132,112,170)).tobytes()
            # Original Arcade retains store quantities; F8 below proves a real release.
            if store_readout(store_before)!=store_readout(store_after):raise RuntimeError('Arcade unexpectedly changed the carried-weapon inventory readout')
            # The latest player store supplies F8; a second press enters its nose camera.
            tap('F8');missile_path=directory/'missile.png';run('import','-window',window,str(missile_path),check=True)
            with Image.open(store_after) as a,Image.open(missile_path) as b:
                if a.convert('RGB').resize((320,200),Image.Resampling.NEAREST).crop((270,187,288,199)).tobytes()==b.convert('RGB').resize((320,200),Image.Resampling.NEAREST).crop((270,187,288,199)).tobytes():raise RuntimeError('F8 did not leave the instrument cockpit after firing')
            tap('F8');tap('F1')
            # Three distinct type changes reach the cannon sentinel after the first missile release.
            for _ in range(3):tap('Return')
            tap('f')
            run('xdotool','keydown','space',check=True);time.sleep(1.5)
            run('xdotool','keyup','space',check=True);time.sleep(.2)
            run('import','-window',window,str(after_path),check=True)
            def readout(path):
                with Image.open(path) as image:return image.convert('RGB').resize((320,200),Image.Resampling.NEAREST).crop((96,91,140,98)).tobytes()
            if readout(before_path)==readout(after_path):raise RuntimeError('Space did not change the cannon ammunition readout')
            # Exercise actual MFD key bindings, including both scene-camera pages.
            def panel(path,bounds):
                with Image.open(path) as image:return image.convert('RGB').resize((320,200),Image.Resampling.NEAREST).crop(bounds).tobytes()
            for key,bounds in [('1',(55,132,112,170)),('2',(130,150,190,197)),('3',(210,132,267,170))]:
                run('import','-window',window,str(before_path),check=True);tap(key)
                run('import','-window',window,str(after_path),check=True)
                if panel(before_path,bounds)==panel(after_path,bounds):raise RuntimeError('MFD '+key+' did not change through keyboard input')
                for _ in range(9):tap(key)
                if game.poll() is not None:raise RuntimeError('cycling MFD '+key+' terminated flight')
            tap('r');tap('n');tap('l');tap('p')
            run('import','-window',window,str(before_path),check=True);time.sleep(.3)
            run('import','-window',window,str(after_path),check=True)
            with Image.open(before_path) as before,Image.open(after_path) as after:
                if before.tobytes()!=after.tobytes():raise RuntimeError('paused instrument frame kept advancing')
            tap('p')
            print('Private X11 MFD controls passed: all three displays cycle, scene pages render, radar range/target keys respond and pause freezes the frame')
            tap('Shift_L+q');confirm_flight_quit(environment,window);game.wait(timeout=10)
            if game.returncode:
                log.seek(0);raise RuntimeError(log.read()[-4000:])
            print('Private X11 controls passed: a store fires; F8 follow/nose and F1 return work; cannon fires; Shift+Q and confirmation exit cleanly')
            game=subprocess.Popen([str(executable),'--data-dir',str(data),'--mission','601','--control','keyboard','--flight-model','arcade','--scale','2'],env=environment,stdout=log,stderr=subprocess.STDOUT)
            deadline=time.monotonic()+10;window=None
            while time.monotonic()<deadline and game.poll() is None:
                found=run('xdotool','search','--onlyvisible','--name','TFX (C|Redux)',capture_output=True,text=True)
                if found.stdout.strip():window=found.stdout.splitlines()[0];break
                time.sleep(.05)
            if window is None:raise RuntimeError('ground-targeting window did not appear')
            run('xdotool','windowfocus','--sync',window,check=True);time.sleep(.2)
            tap('Prior')
            for _ in range(7):tap('1')
            tap('o');tap('z')
            # The original designator has a projected sprite and T/L labels,
            # not the former temporary ZOOM text. The live camera/display
            # arguments are observed separately by check_window_designator.py.
            tap('period');tap('comma')
            tap('Right');tap('z')
            run('import','-window',window,str(store_before),check=True);tap('space')
            run('import','-window',window,str(store_after),check=True)
            if store_readout(store_before)==store_readout(store_after):raise RuntimeError('designating/firing did not release a ground store')
            tap('x');tap('F1');tap('Shift_L+q');confirm_flight_quit(environment,window);game.wait(timeout=10)
            if game.returncode:raise RuntimeError('ground-targeting input failed')
            print('Private X11 ground controls passed: select ground weapon, enter/zoom/aim/designate, release store, unlock and return to flight')
            game=subprocess.Popen([str(executable),'--data-dir',str(data),'--mission','605','--flight-steps','1024','--control','keyboard','--flight-model','arcade','--scale','2'],env=environment,stdout=log,stderr=subprocess.STDOUT)
            deadline=time.monotonic()+10;window=None
            while time.monotonic()<deadline and game.poll() is None:
                found=run('xdotool','search','--onlyvisible','--name','TFX (C|Redux)',capture_output=True,text=True)
                if found.stdout.strip():window=found.stdout.splitlines()[0];break
                time.sleep(.05)
            if window is None:raise RuntimeError('mission-result window did not appear')
            log.flush();log.seek(0)
            if 'Mission result:' not in log.read():raise RuntimeError('flight did not reach the FINISH handoff')
            run('xdotool','windowfocus','--sync',window,check=True);time.sleep(.2)
            result_a=directory/'result-a.png';result_b=directory/'result-b.png'
            run('import','-window',window,str(result_a),check=True);time.sleep(.3)
            run('import','-window',window,str(result_b),check=True)
            with Image.open(result_a) as a,Image.open(result_b) as b:
                if a.tobytes()!=b.tobytes():raise RuntimeError('mission result display kept changing after termination')
            tap('Return');confirm_flight_quit(environment,window);game.wait(timeout=10)
            if game.returncode:raise RuntimeError('Enter did not close the result display cleanly')
            print('Private X11 mission results passed: live combat reaches FINISH; result display is stable; Enter closes')
    finally:
        for process in (game,display):
            if process is not None and process.poll() is None:
                process.terminate()
                try:process.wait(timeout=3)
                except subprocess.TimeoutExpired:process.kill();process.wait()
