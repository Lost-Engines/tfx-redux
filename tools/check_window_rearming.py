#!/usr/bin/env python3
"""Exercise original cheat rearming using real clicks and read-only observation.

Uses a silent disposable install and private display. UI hit points and a legal
tank mount come from the original button/SSD records, without searching by eye.
Checks paused physics, current weapon mass/drag/role, default restoration and
consumed Escape. GDB never changes game state.
"""
import argparse
import json
import os
from pathlib import Path
import select
import struct
import subprocess
import tempfile
import time
from native_reference import NativeReference
from native_planner import NativePlanner
from window_input import move_game_pointer,click_game_pixel,confirm_flight_quit

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--research',type=Path,default=Path('../tfx-reverse'))
p.add_argument('--build',type=Path,default=Path('build'))
a=p.parse_args();research=a.research.resolve();data=research/'installs/cd_1994_09/TFX'
im=NativeReference(data/'TFX.DAT');resources=NativePlanner(data,research)
def hit(region):
 x0,y0,x1,y1=region
 return (x0+x1)//2-5,(y0+y1)//2+8
buttons={}
for index in range(11):
 x0,y0,x1,y1,callback=im.read(0x7f44+index*12,'<4hI')
 if not callback:break
 buttons[callback-im.native(0x70aa4)+0x70aa4]=hit((x0,y0,x1,y1))
tank_row=hit(im.read(0x7fc8+5*12,'<4h'))
raw=resources.read('ss/efa_glob.ssd');table=10+raw[9]*6
collision=table+4+struct.unpack_from('<h',raw,table+4)[0]*2
tank_mask=im.read(0x809c+7*48+23,'<H')[0];mount=None
while struct.unpack_from('<H',raw,collision)[0]==28:
 lo=struct.unpack_from('<3h',raw,collision+2);hi=struct.unpack_from('<3h',raw,collision+8);slot,=struct.unpack_from('<h',raw,collision+14)
 if tank_mask&(1<<slot)and lo[1]<=130<=hi[1]:
  x=(lo[0]+hi[0])//2;z=(lo[2]+hi[2])//2+160
  mount=((640-x)//4,(400-z)//4);break
 collision+=16
assert mount is not None,'no legal original tank mount'
definitions=[(im.read(0x809c+weapon*48+31,'<h')[0],im.read(0x809c+weapon*48+44,'<I')[0],int(im.read(0x809c+weapon*48+20,'<B')[0]==6))for weapon in range(27)]
with tempfile.TemporaryDirectory(prefix='tfx-rearming-window-')as temporary:
 root=Path(temporary);install=root/'TFX';install.mkdir()
 for name in ('TFX.DAT','DID.DAT'):(install/name).symlink_to(data/name)
 original=b'CONTROL=0\r\nMODEL=2\r\nSOUNDFX=0\r\nMUSIC=0\r\nREALISM=0,0,1,1,0,0,0\r\n';config=install/'TFX.CFG';config.write_bytes(original)
 read,write=os.pipe();display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write);game=None
 try:
  if not select.select([read],[],[],5)[0]:raise RuntimeError('private display did not start')
  number=os.read(read,64).decode().strip();os.close(read)
  env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
  for name in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):env.pop(name,None)
  env['ASAN_OPTIONS']=env.get('ASAN_OPTIONS','')+':detect_leaks=0'
  def run(*command):return subprocess.run(command,env=env,check=True,capture_output=True,text=True,timeout=5)
  def tap(key):run('xdotool','keydown',key);time.sleep(.12);run('xdotool','keyup',key);time.sleep(.12)
  script=root/'observe.gdb';script.write_text('''set pagination off
set confirm off
set startup-with-shell off
set print thread-events off
set $inventory = 0
break tfx::weapons::armament::select
commands
silent
set $inventory = this
continue
end
break tfx::flight::player_flight::advance
commands
silent
python
import json,struct
body=gdb.parse_and_eval('this').dereference()['physical']
inventory=gdb.parse_and_eval('$inventory').dereference()['pylons']['_M_elems']
stores=body['mounted_stores']['_M_elems']
rows=[]
for slot in range(10):
    properties=stores[slot]['properties']
    rows.append([int(inventory[slot]['weapon']),int(inventory[slot]['count']),int(stores[slot]['occupied']),int(properties['mass_kg']),struct.unpack('<I',struct.pack('<f',float(properties['drag'])))[0],int(properties['fuel_tank'])])
print('REARM_FRAME '+json.dumps(rows))
end
continue
end
break tfx::flight::ejection_sequence::escape
commands
silent
printf "REARM_ESCAPE_EVENT\\n"
continue
end
break tfx::presentation::arming_controller::choose
commands
silent
printf "REARM_CHOOSE %d\\n", weapon
continue
end
break tfx::presentation::arming_controller::place
commands
silent
printf "REARM_PLACE %u\\n", slot
continue
end
run
''')
  log=root/'game.log'
  with log.open('w')as stream:
   game=subprocess.Popen(['gdb','--quiet','--batch','-x',str(script),'--args',str((a.build/'tfx').resolve()),'--data-dir',str(install),'--mission','501','--scale','2'],env=env,stdout=stream,stderr=subprocess.STDOUT)
   def output():return log.read_text()
   def frames():return [json.loads(line[12:])for line in output().splitlines()if line.startswith('REARM_FRAME ')]
   def wait_for(predicate,message):
    deadline=time.monotonic()+20
    while time.monotonic()<deadline and game.poll()is None:
     if predicate():return
     time.sleep(.03)
    raise RuntimeError(message+'\n'+output()[-5000:])
   wait_for(lambda:bool(frames()),'flight did not start')
   window=run('xdotool','search','--onlyvisible','--name','TFX (C|Redux)').stdout.splitlines()[0];run('xdotool','windowfocus','--sync',window)
   def title():return run('xdotool','getwindowname',window).stdout
   tap('Control_L+d');assert 'Arming'not in title(),'rearming worked without the original cheat'
   run('xdotool','keydown','Shift_R')
   for key in ('p','l','o','p'):tap(key)
   run('xdotool','keyup','Shift_R')
   def open_arming():
    tap('Control_L+d');wait_for(lambda:'Arming'in title(),'cheat Ctrl+D did not open arming')
    time.sleep(1);stopped=len(frames());time.sleep(.2)
    assert len(frames())==stopped,'physics advanced during in-flight arming'
    return stopped
   stopped=open_arming()
   click_game_pixel(env,window,*buttons[0x89b4c]);time.sleep(.2)
   click_game_pixel(env,window,*tank_row);time.sleep(.2)
   wait_for(lambda:'REARM_CHOOSE 7\n'in output(),'the tank thumbnail was not selected')
   # Let the pointer settle before the make: otherwise the original swept
   # segment can select another mount crossed on the way from the thumbnail.
   move_game_pointer(env,window,*mount);time.sleep(.2)
   click_game_pixel(env,window,*mount);time.sleep(.2)
   wait_for(lambda:'REARM_PLACE 'in output(),'the authored legal tank mount was not picked')
   # Placement remains selected after attaching a pair; the original right
   # button returns to the idle controls before DONE can be clicked.
   run('xdotool','mousedown','3');time.sleep(.12);run('xdotool','mouseup','3');time.sleep(.12)
   click_game_pixel(env,window,*buttons[0x89658]);wait_for(lambda:len(frames())>stopped,'flight did not resume after DONE')
   values=frames()[-1]
   assert any(weapon==7 and count and occupied for weapon,count,occupied,*_ in values),('tank was not mounted',mount,values,[line for line in output().splitlines()if line.startswith(('REARM_CHOOSE ','REARM_PLACE '))])
   def check_metadata(values):
    for weapon,count,occupied,mass,drag,tank in values:
     assert occupied==int(count!=0 and weapon!=255),(weapon,count,occupied)
     if occupied:assert (mass,drag,tank)==definitions[weapon],(weapon,mass,drag,tank,definitions[weapon])
   check_metadata(values)
   tap('Shift_L+d');values=frames()[-1];check_metadata(values)
   assert all(weapon!=7 for weapon,count,*_ in values if count),'default restoration kept the custom tank'
   stopped=open_arming();escapes=output().count('REARM_ESCAPE_EVENT\n')
   run('xdotool','keydown','Escape');wait_for(lambda:len(frames())>stopped,'Escape did not resume flight');time.sleep(.25)
   assert output().count('REARM_ESCAPE_EVENT\n')==escapes,'closing arming became an ejection press'
   run('xdotool','keyup','Escape');check_metadata(frames()[-1])
   tap('Shift_L+q');confirm_flight_quit(env,window);game.wait(timeout=15)
   text=output();assert game.returncode==0 and 'exited normally'in text and 'ERROR:'not in text and 'Sanitizer'not in text,text[-6000:]
   assert config.read_bytes()==original,'arming or quit rewrote CFG'
   print('Private X11 cheat rearming passed: gating, paused physics, authored tank placement, current flight mass/drag/role, default restoration, held Escape consumption and clean quit')
 finally:
  for process in (game,display):
   if process is not None and process.poll()is None:
    process.terminate()
    try:process.wait(timeout=3)
    except subprocess.TimeoutExpired:process.kill();process.wait()
