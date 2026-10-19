#!/usr/bin/env python3
"""Observe natural AI warning delivery and both related HUD bindings live.

Uses a private silent installation/display. GDB observes real actor and HUD
arguments without changing game state or supplying an engagement scenario.
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
p.add_argument('--build',type=Path,default=Path('build'))
p.add_argument('--data-dir',type=Path,required=True)
p.add_argument('--mission',default='602')
a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='tfx-engagement-') as temporary:
 directory=Path(temporary);install=directory/'TFX';install.mkdir()
 for name in ('DID.DAT','TFX.DAT'):(install/name).symlink_to((a.data_dir/name).resolve())
 (install/'TFX.CFG').write_bytes(b'CONTROL=0\r\nMODEL=0\r\nSOUNDFX=0\r\nMUSIC=0\r\nREALISM=0,0,1,1,0,0,0\r\n')
 read,write=os.pipe();display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write);game=None
 try:
  if not select.select([read],[],[],5)[0]:raise RuntimeError('private display did not start')
  number=os.read(read,64).decode().strip();os.close(read)
  env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
  for key in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):env.pop(key,None)
  env['ASAN_OPTIONS']=env.get('ASAN_OPTIONS','')+':detect_leaks=0'
  def run(*args):return subprocess.run(args,env=env,check=True,capture_output=True,text=True,timeout=5)
  conditions=[
   ('tfx::missions::engagement_state::observe','warning == tfx::missions::engagement_warning::tracking && targets_player','TRACKING'),
   ('tfx::cockpit::hud::draw','state.fire_permission_notice','PERMISSION_LAMP'),
   ('tfx::cockpit::hud::draw','state.tracked_weapon_category._M_payload._M_engaged','PROJECTILE_LAMP'),
  ]
  script=['set pagination off','set confirm off','set startup-with-shell off','set print thread-events off']
  for index,(function,condition,label) in enumerate(conditions,1):
   script += [f'break {function} if {condition}','commands','silent',f'printf "TFX_ENGAGEMENT_{label}\\n"',f'disable {index}','continue','end']
  debugger=directory/'observe.gdb';debugger.write_text('\n'.join(script+['run'])+'\n');log=directory/'game.log'
  with log.open('w') as output:
   game=subprocess.Popen(['gdb','--quiet','--batch','-x',str(debugger),'--args',str((a.build/'tfx').resolve()),'--data-dir',str(install),'--mission',a.mission,'--scale','2'],env=env,stdout=output,stderr=subprocess.STDOUT)
   def await_text(label,timeout=45):
    marker='TFX_ENGAGEMENT_'+label;deadline=time.monotonic()+timeout
    while time.monotonic()<deadline and game.poll() is None:
     if marker in log.read_text():return
     time.sleep(.05)
    raise RuntimeError('missing '+marker+':\n'+log.read_text()[-6000:])
   deadline=time.monotonic()+30;window=None
   while time.monotonic()<deadline and game.poll() is None:
    windows=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True).stdout.splitlines()
    if windows:window=windows[0];break
    time.sleep(.05)
   if not window:raise RuntimeError(log.read_text()[-6000:])
   run('xdotool','windowfocus','--sync',window)
   for label in ('TRACKING','PERMISSION_LAMP','PROJECTILE_LAMP'):await_text(label)
   run('xdotool','keydown','Shift_L+q');time.sleep(.15);run('xdotool','keyup','Shift_L+q');confirm_flight_quit(env,window);game.wait(timeout=15)
   text=log.read_text();assert game.returncode==0 and 'exited normally' in text and 'ERROR:' not in text and 'Sanitizer' not in text,text[-6000:]
   print('Private silent live engagement passes: natural enemy tracking reaches permission state and cockpit notice, incoming player-targeted projectile reaches launch/guidance lamps, clean exit without scenario mutation')
 finally:
  for process in (game,display):
   if process is not None and process.poll() is None:
    process.terminate()
    try:process.wait(timeout=3)
    except subprocess.TimeoutExpired:process.kill();process.wait()
