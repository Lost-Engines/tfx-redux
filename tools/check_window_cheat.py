#!/usr/bin/env python3
"""Verify real right-Shift PLOP input and its live player-loss gate.

GDB observes, without mutating gameplay. Uses a silent disposable installation
and private display; typing matching P must not pause flight.
"""
import argparse
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time
from window_input import confirm_flight_quit

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--research',type=Path,default=Path('../tfx-reverse'))
p.add_argument('--build',type=Path,default=Path('build'))
a=p.parse_args();data=a.research.resolve()/'installs/cd_1994_09/TFX'
with tempfile.TemporaryDirectory(prefix='tfx-cheat-window-')as temporary:
 root=Path(temporary);install=root/'TFX';install.mkdir()
 for name in ('TFX.DAT','DID.DAT'):(install/name).symlink_to(data/name)
 config=install/'TFX.CFG';original=b'CONTROL=0\r\nMODEL=0\r\nSOUNDFX=0\r\nMUSIC=0\r\n';config.write_bytes(original)
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
break tfx::missions::progress::observe_player_loss
commands
silent
printf "CHEAT_FRAME %d\\n", context.invulnerable
continue
end
run
''')
  log=root/'game.log'
  with log.open('w')as stream:
   game=subprocess.Popen(['gdb','--quiet','--batch','-x',str(script),'--args',str((a.build/'tfx').resolve()),'--data-dir',str(install),'--mission','501','--scale','2'],env=env,stdout=stream,stderr=subprocess.STDOUT)
   def output():return log.read_text()
   def frames():return [int(line[12:])for line in output().splitlines()if line.startswith('CHEAT_FRAME ')]
   def await_frame(value,previous):
    deadline=time.monotonic()+20
    while time.monotonic()<deadline and game.poll()is None:
     values=frames()
     if len(values)>previous and values[-1]==value:return
     time.sleep(.03)
    raise RuntimeError('missing cheat frame '+str(value)+'\n'+output()[-5000:])
   await_frame(0,0)
   window=run('xdotool','search','--onlyvisible','--name','TFX (C|Redux)').stdout.splitlines()[0];run('xdotool','windowfocus','--sync',window)
   for enabled in (1,0):
    run('xdotool','keydown','Shift_R');before=len(frames());tap('p');await_frame(1-enabled,before)
    for key in ('l','o','p'):tap(key)
    run('xdotool','keyup','Shift_R');await_frame(enabled,before)
   tap('Shift_L+q');confirm_flight_quit(env,window);game.wait(timeout=15)
   text=output();assert game.returncode==0 and 'exited normally'in text and 'ERROR:'not in text and 'Sanitizer'not in text,text[-6000:]
   assert config.read_bytes()==original,'cheat or quit rewrote preferences'
   print('Private X11 original cheat passed: real right-Shift PLOP, consumed pause keys, live loss-gate activation/deactivation and clean quit')
 finally:
  for process in (game,display):
   if process is not None and process.poll()is None:
    process.terminate()
    try:process.wait(timeout=3)
    except subprocess.TimeoutExpired:process.kill();process.wait()
