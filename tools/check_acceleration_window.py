#!/usr/bin/env python3
"""Exercise real accelerated-time keys on a private X11 display and disposable install.

GDB only observes the live player input/timing boundary. All controls arrive
through the ordinary window; no game state, outcome or instruction is changed.
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
p.add_argument('--flight-model',choices=('arcade','simple','military-spec','realistic'),default='arcade')
a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='tfx-acceleration-window-') as temporary:
 root=Path(temporary);install=root/'TFX';install.mkdir()
 for name in ('TFX.DAT','DID.DAT'):(install/name).symlink_to((a.data_dir/name).resolve())
 model={'arcade':0,'simple':1,'realistic':2,'military-spec':3}[a.flight_model]
 (install/'TFX.CFG').write_text(f'CONTROL=0\nMODEL={model}\nSOUNDFX=0\nMUSIC=0\nREALISM=0,0,1,1,0,0,0\n')
 observer=root/'timing.gdb'
 observer.write_text('''set pagination off
set confirm off
set startup-with-shell off
set print thread-events off
break tfx::flight::player_flight::advance
commands
silent
printf "TIMING %u %.17g %d %d %d %d %d %d\\n", context.elapsed, context.physics.seconds, context.physics.time_warp, this->basic.flight.landed, context.physics.input.keys.up, context.physics.input.keys.down, context.physics.input.keys.left, context.physics.input.keys.right
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
  for mission in (501,506):
   with (root/f'{mission}.log').open('w+') as log:
    game=subprocess.Popen(['gdb','--quiet','--batch','--return-child-result','-x',str(observer),'--args',str((a.build/'tfx').resolve()),'--data-dir',str(install),'--mission',str(mission),'--control','keyboard','--flight-model',a.flight_model,'--scale','2'],env=env,cwd=root,stdout=log,stderr=subprocess.STDOUT)
    window=None;deadline=time.monotonic()+30
    while time.monotonic()<deadline and game.poll() is None:
     found=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True)
     if found.stdout.strip():window=found.stdout.splitlines()[0];break
     time.sleep(.05)
    if window is None:
     log.seek(0);raise RuntimeError('flight window did not appear:\n'+log.read()[-4000:])
    host('xdotool','windowfocus','--sync',window)
    def records():
     log.seek(0)
     return [list(map(float,line.split()[1:])) for line in log.read().splitlines() if line.startswith('TIMING ')]
    def phase(active,steering=False):
     start=len(records());deadline=time.monotonic()+15
     while time.monotonic()<deadline and game.poll() is None:
      current=records()[start:]
      matching=[row for row in current if bool(row[2])==active and bool(any(row[4:]))==steering]
      if len(matching)>=3:return matching
      time.sleep(.03)
     log.seek(0);raise RuntimeError(f'input phase not reached: {active=} {steering=}\n'+log.read()[-4000:])
    def tap(key):
     host('xdotool','keydown',key);time.sleep(.15);host('xdotool','keyup',key)
    phase(False)
    tap('t');accelerated=phase(True)
    assert all(bool(row[3])==(mission==506) for row in accelerated),('launch ground state',mission,accelerated)
    host('xdotool','keydown','Up');phase(False,True)
    host('xdotool','keyup','Up');phase(True)
    tap('t');phase(False)
    seconds_per_tick=struct.unpack('<f',struct.pack('<f',.02))[0]
    for ticks,seconds,warp,landed,*keys in records():
     raw=round(seconds/seconds_per_tick)
     assert ticks==raw*(5 if warp and not landed else 1),(mission,ticks,seconds,warp,landed,keys)
     checked+=1
    host('xdotool','keydown','Shift_L');host('xdotool','keydown','q');time.sleep(.15)
    host('xdotool','keyup','q');host('xdotool','keyup','Shift_L')
    confirm_flight_quit(env,window);game.wait(timeout=15)
    if game.returncode:
     log.seek(0);raise RuntimeError('flight failed:\n'+log.read()[-4000:])
  print(f'Private X11 accelerated-time checks passed: {a.flight_model}, airborne/runway starts, T toggle, steering suspension/resumption and {checked} live timer/physics samples')
 finally:
  for process in (game,display):
   if process is not None and process.poll() is None:
    process.terminate()
    try:process.wait(timeout=3)
    except subprocess.TimeoutExpired:process.kill();process.wait()
