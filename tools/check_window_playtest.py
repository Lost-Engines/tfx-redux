#!/usr/bin/env python3
"""Exercise focus, held ejection keys and Redux resizing through real X11 input.

GDB observes state only. Runs on a private Xvfb display with a disposable install.
"""
import argparse
import os
from pathlib import Path
import re
import select
import subprocess
import tempfile
import time
from window_input import click_game_pixel

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,default=Path('build'))
p.add_argument('--data-dir',type=Path,default=Path('../tfx-reverse/installs/cd_1994_09/TFX'))
p.add_argument('--redux',action='store_true')
p.add_argument('--fixed-resolution',action='store_true',help='verify --resolution overrides automatic sizing')
p.add_argument('--menu-eject',action='store_true',help='confirm Redux menu ejection instead of testing its shortcut')
p.add_argument('--screenshots',type=Path)
a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='tfx-window-playtest-') as temp:
 root=Path(temp);install=root/'TFX';install.mkdir()
 for name in ('DID.DAT','TFX.DAT'):(install/name).symlink_to((a.data_dir/name).resolve())
 (install/'TFX.CFG').write_text('CONTROL=0\nMODEL=0\nSOUNDFX=0\nMUSIC=0\nREALISM=0,0,1,1,0,0,0\nDETAIL=2\n')
 read,write=os.pipe()
 display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1200x900x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 os.close(write);game=None
 try:
  assert select.select([read],[],[],5)[0]
  number=os.read(read,64).decode().strip();os.close(read)
  env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_SESSION_TYPE='x11')
  for key in ('WAYLAND_DISPLAY','DBUS_SESSION_BUS_ADDRESS'):env.pop(key,None)
  script=root/'observe.gdb';log=root/'game.log'
  script.write_text('''set pagination off
set confirm off
set startup-with-shell off
set print thread-events off
break tfx::flight::ejection_sequence::advance_aircraft
commands
silent
printf "STATE %u %u\\n", this->state.escape_presses, this->state.stage
continue
end
break tfx::platform::window::present
commands
silent
printf "SIZE %d %d\\n", size.x, size.y
continue
end
run
''')
  if not a.redux:
   script.write_text(script.read_text().replace('break tfx::platform::window::present\ncommands\nsilent\nprintf "SIZE %d %d\\n", size.x, size.y\ncontinue\nend\n',''))
  args=[str((a.build/'tfx').resolve()),'--data-dir',str(install),'--mission','501','--music','none','--scale','2']
  if a.fixed_resolution:args.append('--resolution=800x500')
  with log.open('w') as out:
   game=subprocess.Popen(['gdb','--batch','-x',str(script),'--args',*args],env=env,cwd=root,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
   def run(*cmd):return subprocess.run(cmd,env=env,capture_output=True,text=True,check=True,timeout=5)
   def wait(test,reason,timeout=30):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
     if test():return
     if game.poll() is not None:break
     time.sleep(.05)
    raise AssertionError(reason+'\n'+log.read_text()[-3500:])
   windows=[]
   def find_window():
    windows[:]=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True).stdout.splitlines()
    return bool(windows) and 'STATE' in log.read_text()
   wait(find_window,'flight window did not start');window=windows[0]
   run('xdotool','windowfocus','--sync',window)
   def states():return [tuple(map(int,m)) for m in re.findall(r'^STATE (\d+) (\d+)$',log.read_text(),re.M)]
   def tap(key):
    run('xdotool','keydown',key);time.sleep(.3);run('xdotool','keyup',key);time.sleep(.3)
   def title():return run('xdotool','getwindowname',window).stdout
   def screenshot(name):
    if a.screenshots:
     from PIL import ImageGrab
     a.screenshots.mkdir(parents=True,exist_ok=True)
     dims=dict(line.split('=',1) for line in run('xdotool','getwindowgeometry','--shell',window).stdout.splitlines() if '=' in line)
     x,y,w,h=(int(dims[k]) for k in ['X','Y','WIDTH','HEIGHT'])
     ImageGrab.grab(xdisplay=env['DISPLAY']).crop((x,y,x+w,y+h)).save(a.screenshots/(name+'.png'))
   time.sleep(.3)
   run('xdotool','windowfocus','0');time.sleep(.2);before=len(states())
   run('xdotool','keydown','Escape');time.sleep(.6);run('xdotool','keyup','Escape')
   wait(lambda:len(states())>=before+3,'flight stopped on focus loss')
   assert all(s==(0,0) for s in states()),'unfocused Escape reached the aircraft'
   run('xdotool','windowfocus','--sync',window);time.sleep(.2)
   if a.redux:
    run('xdotool','windowsize','--sync',window,'913','617')
    if a.fixed_resolution:
     before=len(states());wait(lambda:len(states())>before+2,'resized fixed-resolution flight stopped')
     assert re.findall(r'^SIZE (\d+) (\d+)$',log.read_text(),re.M)[-1]==('800','500'),'explicit resolution changed with window'
    else:wait(lambda:'SIZE 913 617' in log.read_text(),'automatic render target did not follow a non-integer window size')
    run('xdotool','windowsize','--sync',window,'640','400')
    if not a.fixed_resolution:wait(lambda:log.read_text().rfind('SIZE 640 400')>log.read_text().rfind('SIZE 913 617'),'render target did not shrink')
    tap('Escape');wait(lambda:'Configuration' in title(),'Escape did not open the menu')
    assert all(s==(0,0) for s in states()),'opening menu armed ejection'
    screenshot('flight-menu')
    # EJECT opens a confirmation, and NO returns to the menu.
    click_game_pixel(env,window,280,172)
    wait(lambda:'Eject' in title(),'Eject button did not open confirmation')
    screenshot('eject-confirmation')
    click_game_pixel(env,window,180,112)
    wait(lambda:'Configuration' in title(),'No did not cancel ejection')
    if a.menu_eject:
     click_game_pixel(env,window,280,172);wait(lambda:'Eject' in title(),'Eject did not reopen')
     click_game_pixel(env,window,124,112)
     wait(lambda:any(stage!=0 for presses,stage in states()),'confirmed menu ejection did not eject')
     print('Confirmed menu ejection passed');raise SystemExit(0)
    tap('Escape');wait(lambda:'flight' in title(),'Escape did not resume flight')
   key='ctrl+Escape' if a.redux else 'Escape'
   run('xdotool','keydown',key);time.sleep(1);run('xdotool','keyup',key);time.sleep(.3)
   assert states()[-1]==(1,0),('one held press ejected the pilot',states()[-8:])
   tap(key)
   wait(lambda:any(stage!=0 for presses,stage in states()),'second press did not eject')
   print('Focus/input passed: simulation continues unfocused, unfocused keys ignored, held first press safe, second press ejects'+('; Redux resize and menu/cancel passed' if a.redux else ''))
 finally:
  if game and game.poll() is None:
   import signal
   os.killpg(game.pid,signal.SIGTERM)
   try:game.wait(timeout=5)
   except subprocess.TimeoutExpired:os.killpg(game.pid,signal.SIGKILL);game.wait()
  display.terminate();display.wait(timeout=5)
