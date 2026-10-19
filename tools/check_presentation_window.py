#!/usr/bin/env python3
"""Exercise presentation keys on a private X11 display, never the user's desktop."""
import argparse
from window_input import move_game_pointer
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time

parser=argparse.ArgumentParser(__doc__)
parser.add_argument('--build',type=Path,default=Path('build'))
parser.add_argument('--data-dir',type=Path,required=True)
args=parser.parse_args()
with tempfile.TemporaryDirectory(prefix='tfx-presentation-input-') as temporary:
    read,write=os.pipe()
    display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    os.close(write);game=None
    try:
        if not select.select([read],[],[],5)[0]:raise RuntimeError('private X11 display did not start')
        number=os.read(read,64).decode().strip();os.close(read)
        environment=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
        for name in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):environment.pop(name,None)
        def run(*command):return subprocess.run(command,env=environment,timeout=5,capture_output=True,text=True,check=True)
        for key in ('Escape','shift+q'):
            game=subprocess.Popen([str((args.build/'tfx').resolve()),'--data-dir',str(args.data_dir.resolve()),'--presentation','cdintro','--scale','2'],env=environment,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
            deadline=time.monotonic()+10;window=None
            while time.monotonic()<deadline and game.poll() is None:
                found=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=environment,capture_output=True,text=True,timeout=5)
                if found.stdout.strip():window=found.stdout.splitlines()[0];break
                time.sleep(.05)
            if window is None:raise RuntimeError('presentation window did not appear')
            run('xdotool','windowfocus','--sync',window);time.sleep(1)
            run('xdotool','key','space');time.sleep(1)
            if game.poll() is not None:raise RuntimeError('Space skipped the whole presentation instead of one scene')
            run('xdotool','keydown',key);time.sleep(.12);run('xdotool','keyup',key)
            output=game.communicate(timeout=10)[0]
            if game.returncode:raise RuntimeError(output)
            print('Private X11 presentation: Space advances one scene;',key,'ends the sequence cleanly')
        game=subprocess.Popen([str((args.build/'tfx').resolve()),'--data-dir',str(args.data_dir.resolve()),'--menu','mastopts','--scale','2'],env=environment,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        deadline=time.monotonic()+10;window=None
        while time.monotonic()<deadline and game.poll() is None:
            found=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=environment,capture_output=True,text=True,timeout=5)
            if found.stdout.strip():window=found.stdout.splitlines()[0];break
            time.sleep(.05)
        if window is None:raise RuntimeError('native menu window did not appear')
        run('xdotool','windowfocus','--sync',window);time.sleep(1)
        # Click the FWS box at logical (120,76), independent of window scale.
        move_game_pointer(environment,window,120,76)
        run('xdotool','mousedown','1');time.sleep(1)
        if game.poll() is not None:raise RuntimeError('menu exited before mouse release')
        run('xdotool','mouseup','1')
        output=game.communicate(timeout=10)[0]
        if game.returncode or 'Menu choice: 1' not in output:raise RuntimeError('native menu did not choose FWS through the aspect-correct pointer: '+output)
        print('Private X11 menu: original animated artwork, aspect-correct FWS click, hold until release, fade and exit passed')
    finally:
        if game and game.poll() is None:game.kill();game.wait()
        display.terminate();display.wait(timeout=5)
