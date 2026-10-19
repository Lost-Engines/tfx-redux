#!/usr/bin/env python3
"""Complete the first FWS engagement using normal controls on a private display.

GDB observes owners, targeting and results; all controls arrive through X11.
--campaign restores a disposable pilot immediately before Hop 1, then checks
arming, combat, debrief and the original next-hop handoff. The supplied save
sets no combat outcome. No original installation is modified.
"""
import argparse
import os
from pathlib import Path
import select
import struct
import subprocess
import tempfile
import time
from window_input import confirm_flight_quit

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--data-dir',type=Path,default=Path('../tfx-reverse/installs/cd_1994_09/TFX'))
p.add_argument('--build',type=Path,default=Path('build'))
p.add_argument('--campaign',action='store_true')
p.add_argument('--recording',action='store_true',help='record Hop 1 and observe WNN introductions/replay before Hop 2')
p.add_argument('--research',type=Path,default=Path('../tfx-reverse'))
p.add_argument('--flight-model',choices=('arcade','simple','military-spec','realistic'),default='arcade')
a=p.parse_args()
if a.recording and not a.campaign:p.error('--recording requires --campaign')
with tempfile.TemporaryDirectory(prefix='tfx-training-engagement-') as temporary:
 root=Path(temporary);install=root/'TFX';install.mkdir()
 for name in ('TFX.DAT','DID.DAT'):(install/name).symlink_to((a.data_dir/name).resolve())
 model={'arcade':0,'simple':1,'realistic':2,'military-spec':3}[a.flight_model]
 (install/'TFX.CFG').write_text(f'CONTROL=0\nMODEL={model}\nSOUNDFX=0\nMUSIC=0\nREALISM=0,0,1,1,0,0,0\n')
 if a.campaign:
  import json
  program=next(p for p in json.loads((a.research/'analysis/presentations/programs.json').read_text())if p['name']=='tfx')
  function=next(i for i,f in enumerate(program['functions'])if f['name']=='fws')
  values={'pilotname':'TEST','firstname':'TRAINING','callsign':'SUCCESS','rank':1,'mission':501,'training':1}
  pilot=bytearray()
  for variable in program['variables']:
   value=values.get(variable['name'],'' if variable['string_value']else 0)
   pilot.extend(str(value).encode().ljust(21,b'\0')if variable['string_value']else struct.pack('<I',value))
  pilot.extend(bytes((30-len(program['variables']))*4));pilot.extend(struct.pack('<3H',0,function,5))
  (install/'TFXSAV.0').write_bytes(pilot)
 observer=root/'timing.gdb'
 observer.write_text('''set pagination off
set confirm off
set startup-with-shell off
set print thread-events off
break tfx::platform::fly_mission
commands
silent
python
import json, struct
options=gdb.parse_and_eval('options')
values=gdb.parse_and_eval('campaign')
if values.type.code==gdb.TYPE_CODE_REF:values=values.referenced_value()
printer=gdb.default_visualizer(values)
if printer is None:raise RuntimeError('GNU std::map printer is required for read-only campaign observation')
children=list(printer.children())
numbers={children[i][1]['_M_dataplus']['_M_p'].string():int(children[i+1][1]) for i in range(0,len(children),2)}
entry_mission=options['mission']['_M_dataplus']['_M_p'].string()
inherited=options['mission_state']
inherited_values={'position':[int(inherited['position'][axis])for axis in ('x','y','z')],'speed_factor':struct.unpack('<I',struct.pack('<f',float(inherited['speed_factor'])))[0],'weapons':int(inherited['weapons'])}
inventory=options['launch_inventory']['_M_elems']
inherited_values['pylons']=[[int(inventory[i][field])for field in ('weapon','count','visible')]for i in range(11)]
print('ENTRY '+json.dumps({'mission':entry_mission,'values':numbers,'inherited':inherited_values}))
end
continue
end
set $seeker = 0
set $armament = 0
set $progress = 0
set $launch_player = 0
set $pilot_page = -1
break tfx::presentation::pilot_panel::advance
commands
silent
if !this->entering && (int)this->current != $pilot_page
set $pilot_page = (int)this->current
printf "PILOT_READY %d\\n", $pilot_page
end
continue
end
break tfx::flight::player_flight::advance
commands
silent
set $launch_player = this
continue
end
break tfx::sensors::air_lock::begin_mission
commands
silent
set $seeker = this
continue
end
break tfx::weapons::armament::select
commands
silent
set $armament = this
continue
end
break tfx::missions::progress::begin_frame
commands
silent
set $progress = this
continue
end
break tfx::platform::replay_recording
commands
silent
printf "RECORDING_REPLAY\\n"
continue
end
break tfx::world::scene::replay
commands
silent
printf "RECORDING_FRAME\\n"
continue
end
break tfx::missions::progress::finish_frame
commands
silent
python
import json
progress=gdb.parse_and_eval('$progress').dereference()
seeker=gdb.parse_and_eval('$seeker').dereference()
armament=gdb.parse_and_eval('$armament').dereference()
slot=int(armament['selected'])
weapon=int(armament['pylons']['_M_elems'][slot]['weapon']) if 0<=slot<10 else -1
print('COMBAT '+json.dumps([int(progress['results']['enemy_aircraft_down']),int(progress['results']['completed']),int(seeker['guidance']['_M_payload']['_M_engaged']),int(seeker['lock_ticks']),weapon]))
flight=gdb.parse_and_eval('$launch_player').dereference()['basic']['flight']
print('FLIGHT_STATE '+json.dumps({'mission':entry_mission,'position':[int(flight['integer_position'][axis])for axis in ('x','y','z')],'speed_factor':struct.unpack('<I',struct.pack('<f',float(flight['speed_mach'])))[0],'pylons':[[int(armament['pylons']['_M_elems'][i][field])for field in ('weapon','count','visible')]for i in range(11)]}))
end
continue
end
run
''')
 read,write=os.pipe()
 display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 os.close(write);game=None
 try:
  if not select.select([read],[],[],5)[0]:raise RuntimeError('private X11 display did not start')
  number=os.read(read,64).decode().strip();os.close(read)
  env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
  env['ASAN_OPTIONS']=env.get('ASAN_OPTIONS','')+':detect_leaks=0'
  for name in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):env.pop(name,None)
  def host(*command):return subprocess.run(command,env=env,timeout=5,check=True,capture_output=True,text=True)
  checked=0
  for mission in (501,):
   log_path=root/f'{mission}.log'
   with log_path.open('w') as log:
    # Separate read handles keep observation from seeking GDB's shared
    # stdout file description while it writes the next record.
    def output():return log_path.read_text()
    game=subprocess.Popen(['gdb','--quiet','--batch','--return-child-result','-x',str(observer),'--args',str((a.build/'tfx').resolve()),'--data-dir',str(install),*(['--skip-intro']if a.campaign else ['--mission',str(mission)]),'--control','keyboard','--flight-model',a.flight_model,'--scale','2'],env=env,cwd=root,stdout=log,stderr=subprocess.STDOUT)
    window=None;deadline=time.monotonic()+30
    while time.monotonic()<deadline and game.poll() is None:
     found=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True)
     if found.stdout.strip():window=found.stdout.splitlines()[0];break
     time.sleep(.05)
    if window is None:
     raise RuntimeError('flight window did not appear:\n'+output()[-4000:])
    host('xdotool','windowfocus','--sync',window)
    def records():
     import json
     text=output();lines=text.splitlines()
     # GDB writes JSON to a live file; its last line may still be in flight.
     if text and not text.endswith('\n'):lines=lines[:-1]
     return [json.loads(line[len('COMBAT '):]) for line in lines if line.startswith('COMBAT ')]
    def wait_for(predicate,label,timeout=30):
     deadline=time.monotonic()+timeout
     while time.monotonic()<deadline and game.poll() is None:
      current=records()
      if current and predicate(current[-1]):return current[-1]
      time.sleep(.05)
     raise RuntimeError(label+': '+str(current[-4:])+'\n'+output()[-4000:])
    def tap(key):
     host('xdotool','keydown',key);time.sleep(.15);host('xdotool','keyup',key)
    def await_text(text,timeout=30):
     deadline=time.monotonic()+timeout
     while time.monotonic()<deadline and game.poll() is None:
      if text in output():return
      time.sleep(.025)
     raise RuntimeError('missing '+repr(text)+'\n'+output()[-5000:])
    if a.campaign:
     await_text('PILOT_READY 0\n')
     tap('Down');tap('Return')
     await_text('PILOT_READY 2\n')
     tap('Return')
     deadline=time.monotonic()+30
     while time.monotonic()<deadline and game.poll() is None:
      title=host('xdotool','getwindowname',window).stdout
      if 'Arming' in title:break
      time.sleep(.05)
     if 'Arming' not in title:
      raise RuntimeError('arming was not reached: '+title+'\n'+output()[-5000:])
     time.sleep(1)
     for x in (202,394):
      host('xdotool','mousemove','--window',window,str(x),'398');time.sleep(.15)
      host('xdotool','mousedown','1');time.sleep(.15);host('xdotool','mouseup','1');time.sleep(.2)
    wait_for(lambda row:True,'initial flight')
    if a.recording:tap('alt+v')
    tap('Return')
    tap('t')
    lock=wait_for(lambda row:row[2] and row[3]==0 and row[4]==4,'AMRAAM lock')
    print('LOCK',lock,flush=True)
    tap('space')
    kill=wait_for(lambda row:row[0]>=1,'enemy loss')
    print('KILL',kill,flush=True)
    complete=wait_for(lambda row:row[1],'objective complete')
    print('COMPLETE',complete,flush=True)
    deadline=time.monotonic()+20
    while time.monotonic()<deadline:
     if 'Mission result:' in output():break
     time.sleep(.05)
    text=output()
    assert 'Mission result:' in text and 'outcome 0' in text,text[-4000:]
    checked=len(records())
    if a.recording:
     # The two original animations run to completion. No game state or
     # timing is replaced to make the WNN/replay host reachable.
     titles=set();deadline=time.monotonic()+20
     while time.monotonic()<deadline and game.poll() is None:
      title=host('xdotool','getwindowname',window).stdout
      for name in ('wwn','prtv2','Flight recording'):
       if name in title:titles.add(name)
      if 'Flight recording' in titles:
       if 'RECORDING_FRAME' in output():break
      time.sleep(.025)
     assert titles=={'wwn','prtv2','Flight recording'},titles
     from native_recording import decode_recording
     frames=decode_recording((install/'video.rec').read_bytes())
     assert len(frames)>20,('incomplete live recording',len(frames))
     tap('space')
     print('REPLAY',len(frames),'streamed frames; both WNN introductions and first replay frame observed',flush=True)
    if a.campaign:
     deadline=time.monotonic()+30
     while time.monotonic()<deadline and game.poll() is None:
      title=host('xdotool','getwindowname',window).stdout
      if 'Arming' in title:break
      tap('space');time.sleep(.1)
     assert 'Arming' in title,('following mission not reached',title)
     import json
     observation=output().splitlines()
     entries=[json.loads(line[len('ENTRY '):])for line in observation if line.startswith('ENTRY ')]
     assert len(entries)==2 and entries[0]['mission']=='miss501' and entries[1]['mission']=='miss502',entries
     ended=[json.loads(line[len('FLIGHT_STATE '):])for line in observation if line.startswith('FLIGHT_STATE ')]
     ended=next(state for state in reversed(ended)if state['mission']=='miss501')
     inherited=entries[1]['inherited']
     assert inherited['position']==ended['position'] and inherited['speed_factor']==ended['speed_factor'],('retained live flight state',inherited,ended)
     assert inherited['weapons']==0,('Hop 1 WEAPONS index',inherited)
     assert inherited['pylons']==ended['pylons'],('retained live inventory',inherited,ended)
     continued=entries[1]['values']
     assert continued['a2akills']>=1 and continued['num_miss']>=1 and continued['score']>0 and continued['mission']==502,continued
     # FWS's SPOS belongs to training graduation, not each hop. Do not invent
     # a per-hop autosave merely to satisfy this integration check.
     assert (install/'TFXSAV.0').read_bytes()==pilot,'unexpected per-hop save rewrite'
     time.sleep(1);tap('Escape')
     wait_for(lambda row:row[0]==0,'next hop enters flight')
    host('xdotool','keydown','Shift_L');host('xdotool','keydown','q');time.sleep(.15)
    host('xdotool','keyup','q');host('xdotool','keyup','Shift_L')
    confirm_flight_quit(env,window);game.wait(timeout=15)
    if game.returncode:
     raise RuntimeError('flight failed:\n'+output()[-4000:])
  print(f'Private X11 training engagement passed: {checked} real frames, AMRAAM lock/release, enemy loss, objective completion, successful FINISH'+('; original pilot/arming/debrief, next-hop campaign values and retained position/Mach/WEAPONS/inventory state'if a.campaign else ''))
 finally:
  for process in (game,display):
   if process is not None and process.poll() is None:
    process.terminate()
    try:process.wait(timeout=3)
    except subprocess.TimeoutExpired:process.kill();process.wait()
