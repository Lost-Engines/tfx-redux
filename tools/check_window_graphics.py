#!/usr/bin/env python3
"""Check live Redux graphics with real input on a silent private X11 display.

GDB only observes drawing and simulation calls. All settings and captures go
into temporary/private paths; source assets are read through symlinks.
"""
import argparse
import math
import os
from pathlib import Path
import select
import signal
import subprocess
import tempfile
import time
from PIL import ImageGrab

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build',type=Path,default=Path('build'))
parser.add_argument('--data-dir',type=Path,default=Path('../tfx-reverse/installs/cd_1994_09/TFX'))
parser.add_argument('--captures',type=Path,default=Path('/tmp/tfx-graphics-menu'))
a=parser.parse_args();a.captures.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='tfx-graphics-window-') as temporary:
    root=Path(temporary);install=root/'TFX';install.mkdir()
    for name in ('TFX.DAT','DID.DAT'):(install/name).symlink_to((a.data_dir/name).resolve())
    (install/'TFX.CFG').write_text('CONTROL=0\nMODEL=3\nSOUNDFX=0\nMUSIC=0\nREALISM=2,0,1,1,1,0,0\nDETAIL=2\n')
    graphics=install/'REDUX.CFG'
    graphics.write_text('RESOLUTION=960x600\nFOV=80\nDRAW_DISTANCE=8\nLOD_SCALE=0.5\nWINDOW_SCALE=1\n')
    read,write=os.pipe()
    display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1280x900x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    os.close(write);game=None
    try:
        assert select.select([read],[],[],5)[0], 'private display did not start'
        number=os.read(read,64).decode().strip();os.close(read)
        env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_SESSION_TYPE='x11')
        for key in ('WAYLAND_DISPLAY','DBUS_SESSION_BUS_ADDRESS'):env.pop(key,None)
        debugger=root/'observe.gdb';log=root/'game.log'
        debugger.write_text('''set pagination off
set confirm off
set startup-with-shell off
set print thread-events off
break tfx::world::scene::draw
commands
silent
if pass == tfx::world::scene_pass::primary
printf "DRAW %d %d %.10f %u %.10f\\n", renderer.output.size.x, renderer.output.size.y, view.focal_distance, radius, this->visual_lod_scale
end
continue
end
break tfx::flight::player_flight::advance
commands
silent
printf "TICK\\n"
continue
end
break tfx::platform::window::present
commands
silent
printf "FRAME\\n"
continue
end
run
''')
        def run(*args):return subprocess.run(args,env=env,check=True,capture_output=True,text=True,timeout=5)
        def output():return log.read_text()
        def wait(predicate,message,timeout=35):
            end=time.monotonic()+timeout
            while time.monotonic()<end and game.poll() is None:
                if predicate():return
                time.sleep(.04)
            raise AssertionError(message+'\n'+output()[-3500:])
        def frames(n=3):
            start=output().count('FRAME\n')
            wait(lambda:output().count('FRAME\n')>=start+n,'window stopped presenting')
        def tap(key):
            run('xdotool','keydown',key);frames(2);run('xdotool','keyup',key);frames(2)
        def draws():return [list(map(float,line.split()[1:])) for line in output().splitlines() if line.startswith('DRAW ')]
        def dimensions():
            data=dict(line.split('=',1) for line in run('xdotool','getwindowgeometry','--shell',window).stdout.splitlines() if '=' in line)
            return int(data['WIDTH']),int(data['HEIGHT'])
        automatic=False
        def click(x,y,closing=False):
            width,height=dimensions();scale=min(width/320,height/200)
            if not automatic and scale>=1:scale=int(scale)
            left=(width-int(320*scale))//2;top=height-(height-int(200*scale))//2-int(200*scale)
            run('xdotool','mousemove','--window',window,str(round(left+x*scale)),str(round(top+y*scale)))
            run('xdotool','mousedown','1')
            if closing:time.sleep(.15)
            else:frames(2)
            run('xdotool','mouseup','1')
            if not closing:frames(2)
        def title():return run('xdotool','getwindowname',window).stdout
        def edit(row,text):
            click(180,60+16*row)
            run('xdotool','type','--clearmodifiers','--delay','20',text)
            frames(2);tap('Return')
        def open_graphics():
            tap('Escape');wait(lambda:'Configuration' in title(),'configuration not shown')
            click(280,36);wait(lambda:'Graphics' in title(),'graphics not shown')
        def check_draw(width,height,fov,radius,lod):
            expected=width/(2*math.tan(math.radians(fov)/2))
            wait(lambda:bool(draws()) and draws()[-1][:2]==[width,height] and math.isclose(draws()[-1][2],expected,rel_tol=1e-7) and draws()[-1][3:]==[radius,lod], 'new graphics did not reach the scene renderer')
        def stop():
            global game
            if game and game.poll() is None:
                os.killpg(game.pid,signal.SIGTERM)
                try:game.wait(timeout=5)
                except subprocess.TimeoutExpired:os.killpg(game.pid,signal.SIGKILL);game.wait()
            game=None
        for session in range(2):
            with log.open('w') as stream:
                extra=['--resolution','800x500','--fov','70','--scale','1'] if session==0 else ['--resolution','640x400','--scale','1']
                game=subprocess.Popen(['gdb','--batch','-x',str(debugger),'--args',str((a.build/'tfx').resolve()),'--data-dir',str(install),'--mission','702',*extra],cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,start_new_session=True)
                wait(lambda:len(draws())>=4,'flight did not start')
                window=run('xdotool','search','--onlyvisible','--name','TFX Redux').stdout.splitlines()[0]
                run('xdotool','windowfocus','--sync',window);automatic=False
                if session==0:
                    check_draw(800,500,70,8,.5)
                    saved=graphics.read_bytes();tap('Pause');frames(3);ticks=output().count('TICK\n')
                    open_graphics();edit(2,'999');click(260,172)
                    assert 'Graphics' in title() and graphics.read_bytes()==saved, 'invalid edit was applied or saved'
                    edit(2,'90');edit(0,'1001x603');edit(3,'4');edit(4,'0.25')
                    ImageGrab.grab(xdisplay=env['DISPLAY']).save(a.captures/'graphics.png')
                    click(260,172);check_draw(1001,603,90,4,.25)
                    assert output().count('TICK\n')==ticks, 'Apply unpaused flight'
                    assert dimensions()==(800,500), 'resolution/FOV edit unexpectedly resized the window'
                    text=graphics.read_text();assert 'RESOLUTION=1001x603' in text and 'FOV=90' in text and 'DRAW_DISTANCE=4' in text and 'LOD_SCALE=0.25' in text
                    ImageGrab.grab(xdisplay=env['DISPLAY']).save(a.captures/'applied.png')
                    open_graphics();edit(0,'AUTO');edit(3,'12');click(260,172);automatic=True
                    check_draw(800,500,90,12,.25)
                    run('xdotool','windowsize',window,'973','617');frames(5);check_draw(973,617,90,12,.25)
                    saved=graphics.read_bytes();open_graphics();edit(2,'40');click(160,172)
                    wait(lambda:'Configuration' in title(),'Cancel did not return to configuration');tap('Escape');frames(4)
                    assert graphics.read_bytes()==saved,'Cancel saved graphics'
                    check_draw(973,617,90,12,.25)
                    assert output().count('TICK\n')==ticks,'paused editing/resizing advanced simulation'
                    stop()
                else:
                    # Command-line resolution/scale override only those fields;
                    # the runtime FOV, distance and LOD survive the restart.
                    check_draw(640,400,90,12,.25)
                    open_graphics();click(70,172);click(260,172);automatic=True
                    width,height=dimensions();check_draw(width,height,64.01076641616699,16,.1)
                    assert (width,height)==(1280,900),'Defaults did not fit the desktop'
                    text=graphics.read_text();assert 'RESOLUTION=AUTO' in text and 'WINDOW_SCALE=FIT' in text and 'LOD_SCALE=0.1' in text
                    tap('Shift_L+q');wait(lambda:'Quit' in title(),'quit prompt absent');click(132,108,closing=True)
                    game.wait(timeout=15)
                    assert game.returncode==0 and 'exited normally' in output(),output()[-2500:]
                    game=None
        print('Private X11 graphics passed: live size/FOV/radius/LOD, invalid values, Apply/Cancel, pause, automatic resize, saved reload, per-option CLI overrides, Defaults/window fit and clean exit')
    finally:
        if game is not None and game.poll() is None:
            os.killpg(game.pid,signal.SIGTERM)
            try:game.wait(timeout=5)
            except subprocess.TimeoutExpired:os.killpg(game.pid,signal.SIGKILL);game.wait()
        display.terminate();display.wait(timeout=5)
