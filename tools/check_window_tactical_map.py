#!/usr/bin/env python3
"""Exercise the M map using real keys, a private display and read-only GDB.

Checks paused physics, held-key dismissal, paused-flight return and normal map
colours while infrared flight remains selected. Original installations stay
read-only; the game uses a disposable linked installation and silent settings.
"""
import argparse
import json
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time
from PIL import Image
from window_input import confirm_flight_quit

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--research',type=Path,default=Path('../tfx-reverse'))
p.add_argument('--build',type=Path,default=Path('build'))
a=p.parse_args();research=a.research.resolve();data=research/'installs/cd_1994_09/TFX'
captures=json.loads((research/'analysis/lighting/environments.json').read_text())['environments']
night=next(case for case in captures if case['environment']=='yugos_n')
bank=next(bank for bank in night['banks']if bank['data_offset']=='0xea88')
raw=(research/'extracted/cd_1994_09/resources'/bank['path']).read_bytes()
infrared={tuple((value*255+31)//63 for value in raw[i:i+3])for i in range(0,768,3)}
with tempfile.TemporaryDirectory(prefix='tfx-tactical-map-window-')as temporary:
 root=Path(temporary);install=root/'TFX';install.mkdir()
 for name in ('TFX.DAT','DID.DAT'):(install/name).symlink_to((data/name).resolve())
 config=install/'TFX.CFG';original=b'CONTROL=0\r\nMODEL=0\r\nSOUNDFX=0\r\nMUSIC=0\r\nREALISM=0,0,1,1,0,0,0\r\n';config.write_bytes(original)
 read,write=os.pipe();display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write);game=None
 try:
  if not select.select([read],[],[],5)[0]:raise RuntimeError('private display did not start')
  number=os.read(read,64).decode().strip();os.close(read)
  env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
  for key in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):env.pop(key,None)
  env['ASAN_OPTIONS']=env.get('ASAN_OPTIONS','')+':detect_leaks=0'
  def run(*command):return subprocess.run(command,env=env,check=True,capture_output=True,text=True,timeout=5)
  def tap(key):run('xdotool','keydown',key);time.sleep(.12);run('xdotool','keyup',key);time.sleep(.12)
  script=root/'observe.gdb';script.write_text('''set pagination off
set confirm off
set startup-with-shell off
set print thread-events off
break tfx::flight::player_flight::advance
commands
silent
printf "MAP_TICK\\n"
continue
end
break tfx::cockpit::tactical_map::open
commands
silent
printf "MAP_OPEN\\n"
continue
end
break tfx::platform::show_tactical_map
commands
silent
python
supplied = bytes(gdb.selected_inferior().read_memory(int(gdb.parse_and_eval('&colours')), 768))
try:
    bank = gdb.newest_frame().older().read_var('palette_lighting')['current']
    raw = bytes(int(bank['_M_elems'][index]) for index in range(768))
except gdb.error:
    print('MAP_BOUNDARY: optimized caller palette unavailable; checking presented RGB and infrared return only')
    expected = supplied
else:
    expected = bytes(((value & 63) * 255 + 31) // 63 for value in raw)
    assert supplied == expected, 'map palette is stale while infrared is displayed'
print('MAP_PALETTE ' + expected.hex())
end
continue
end
break tfx::flight::ejection_sequence::escape
commands
silent
printf "MAP_ESCAPE_EVENT\\n"
continue
end
break tfx::weapons::armament::select
commands
silent
printf "MAP_WEAPON_SELECTION\\n"
continue
end
run
''')
  log=root/'game.log'
  with log.open('w')as stream:
   game=subprocess.Popen(['gdb','--quiet','--batch','-x',str(script),'--args',str((a.build/'tfx').resolve()),'--data-dir',str(install),'--mission','501','--environment','yugos_n','--scale','2'],env=env,stdout=stream,stderr=subprocess.STDOUT)
   def output():return log.read_text()
   def count(marker):return output().count(marker+'\n')
   def await_count(marker,minimum):
    deadline=time.monotonic()+20
    while time.monotonic()<deadline and game.poll()is None:
     if count(marker)>=minimum:return
     time.sleep(.03)
    raise RuntimeError('missing '+marker+'\n'+output()[-5000:])
   await_count('MAP_TICK',1)
   window=run('xdotool','search','--onlyvisible','--name','TFX (C|Redux)').stdout.splitlines()[0];run('xdotool','windowfocus','--sync',window)
   def title():return run('xdotool','getwindowname',window).stdout
   def open_map():
    wanted=count('MAP_OPEN')+1;tap('m');await_count('MAP_OPEN',wanted)
    deadline=time.monotonic()+10
    while time.monotonic()<deadline and 'Tactical map'not in title():time.sleep(.02)
    assert 'Tactical map'in title(),output()[-5000:]
    stopped=count('MAP_TICK');time.sleep(.25);assert count('MAP_TICK')==stopped,'physics advanced in the map'
    return stopped
   def capture(name):
    path=root/name;run('import','-window',window,str(path))
    with Image.open(path)as image:return image.convert('RGB').resize((320,200),Image.Resampling.NEAREST)
   def check_map_colours(image):
    palette=bytes.fromhex(next(line[12:]for line in reversed(output().splitlines())if line.startswith('MAP_PALETTE ')))
    expected=set(zip(palette[0::3],palette[1::3],palette[2::3]))
    raw=image.tobytes();actual=set(zip(raw[0::3],raw[1::3],raw[2::3]))
    assert actual<=expected,('map does not use the current normal palette',actual-expected)
    assert len(actual)>10 and actual-infrared,'map artwork or normal colours are missing'
   stopped=open_map();normal=capture('normal-map.png')
   check_map_colours(normal)
   run('xdotool','keydown','Escape');await_count('MAP_TICK',stopped+1);time.sleep(.35)
   assert count('MAP_ESCAPE_EVENT')==0,'the map dismissal became an ejection key'
   run('xdotool','keyup','Escape');time.sleep(.1)
   tap('Shift_L+n');tap('u');stopped=open_map();night_map=capture('infrared-map.png')
   check_map_colours(night_map)
   opened=count('MAP_OPEN');run('xdotool','keydown','m');await_count('MAP_TICK',stopped+1);time.sleep(.35)
   assert count('MAP_OPEN')==opened,'held M immediately reopened the map'
   run('xdotool','keyup','m');time.sleep(.1)
   flight=capture('infrared-flight.png')
   pixels=flight.tobytes()
   assert all(pixel in infrared for pixel in zip(pixels[0::3],pixels[1::3],pixels[2::3])),'map return lost infrared flight colours'
   tap('p');paused=count('MAP_TICK');time.sleep(.25);assert count('MAP_TICK')==paused
   open_map();selections=count('MAP_WEAPON_SELECTION');tap('Return');time.sleep(.25)
   assert count('MAP_TICK')==paused,'leaving the map unpaused the flight'
   assert count('MAP_WEAPON_SELECTION')==selections,'the dismissal selected a weapon'
   tap('p');await_count('MAP_TICK',paused+1)
   tap('Shift_L+q');confirm_flight_quit(env,window);game.wait(timeout=15)
   text=output();assert game.returncode==0 and 'exited normally'in text and 'ERROR:'not in text and 'Sanitizer'not in text,text[-6000:]
   assert config.read_bytes()==original,'the map or quit prompt rewrote preferences'
   for line in dict.fromkeys(line for line in text.splitlines()if line.startswith('MAP_BOUNDARY:')):print(line)
   print('Private X11 tactical map passed: original artwork, paused physics, normal/infrared palette handoff, held Escape/M/Enter consumption, paused-flight return and clean quit')
 finally:
  for process in (game,display):
   if process is not None and process.poll()is None:
    process.terminate()
    try:process.wait(timeout=3)
    except subprocess.TimeoutExpired:process.kill();process.wait()
