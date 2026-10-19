#!/usr/bin/env python3
"""Check square-pixel window sizing and resized menu input on a private display."""
import argparse
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time
from PIL import Image
from window_input import move_game_pointer

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build',type=Path,default=Path('build'))
parser.add_argument('--data-dir',type=Path,default=Path('../tfx-reverse/installs/cd_1994_09/TFX'))
args=parser.parse_args()
with tempfile.TemporaryDirectory(prefix='tfx-square-pixels-') as temporary:
    root=Path(temporary)
    read,write=os.pipe()
    display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','2048x1536x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    os.close(write);game=None
    try:
        if not select.select([read],[],[],5)[0]:raise RuntimeError('private display did not start')
        number=os.read(read,64).decode().strip();os.close(read)
        env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
        for name in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):env.pop(name,None)
        def run(*command):return subprocess.run(command,env=env,timeout=10,capture_output=True,text=True,check=True)
        def geometry(window):
            fields=dict(line.split('=',1) for line in run('xdotool','getwindowgeometry','--shell',window).stdout.splitlines() if '=' in line)
            return int(fields['WIDTH']),int(fields['HEIGHT'])
        def squares(window,scale):
            screenshot=root/'window.png';run('import','-window',window,str(screenshot))
            image=Image.open(screenshot).convert('RGB');width,height=image.size
            left,bottom=(width-320*scale)//2,(height-200*scale)//2
            top=height-bottom-200*scale
            content=image.crop((left,top,left+320*scale,top+200*scale))
            small=content.resize((320,200),Image.Resampling.NEAREST)
            assert small.resize(content.size,Image.Resampling.NEAREST).tobytes()==content.tobytes(),(image.size,scale,'non-square or interpolated game pixels')
            assert len(small.getcolors(64000))>5,'empty capture'
            for box in ((0,0,width,top),(0,top+200*scale,width,height),(0,top,left,top+200*scale),(left+320*scale,top,width,top+200*scale)):
                if box[2]>box[0] and box[3]>box[1]:assert image.crop(box).getextrema()==((0,0),(0,0),(0,0)),'non-black resize margin'
        for requested in (1,2,6,0):
            game=subprocess.Popen([str((args.build/'tfx').resolve()),'--data-dir',str(args.data_dir.resolve()),'--menu','mastopts','--scale',str(requested)],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
            deadline=time.monotonic()+10;window=None
            while time.monotonic()<deadline and game.poll() is None:
                result=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True,timeout=5)
                if result.stdout.strip():window=result.stdout.splitlines()[0];break
                time.sleep(.05)
            if window is None:raise RuntimeError('menu did not open')
            run('xdotool','windowfocus','--sync',window);run('xdotool','mousemove','2040','1530');time.sleep(.8)
            scale=requested or 6
            assert geometry(window)==(320*scale,200*scale),(requested,geometry(window))
            squares(window,scale)
            if requested==6:
                run('xdotool','windowsize',window,'997','743');time.sleep(.3)
                assert geometry(window)==(997,743)
                squares(window,3)
            move_game_pointer(env,window,120,76)
            run('xdotool','mousedown','1');time.sleep(.15);run('xdotool','mouseup','1')
            output=game.communicate(timeout=10)[0]
            assert game.returncode==0 and 'Menu choice: 1' in output,(requested,output)
            print(f'Square pixels: --scale={requested}, uniform {scale}x{scale} pixel blocks, centred resize margins and real FWS click passed')
    finally:
        if game and game.poll() is None:game.kill();game.wait()
        display.terminate();display.wait(timeout=5)
