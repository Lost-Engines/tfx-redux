#!/usr/bin/env python3
"""Observe real defensive keys, live HUD state and guided-motion ECM inputs.

Uses a private silent installation and X11 display. GDB only observes arguments;
it never modifies game state. Native key/guidance arithmetic is checked separately.
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
a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='tfx-defence-') as temporary:
 directory=Path(temporary);install=directory/'TFX';install.mkdir()
 for name in ('DID.DAT','TFX.DAT'):(install/name).symlink_to((a.data_dir/name).resolve())
 (install/'TFX.CFG').write_bytes(b'CONTROL=0\r\nMODEL=0\r\nSOUNDFX=0\r\nMUSIC=0\r\nREALISM=0,0,1,1,0,0,0\r\n')
 read,write=os.pipe();display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write);game=None
 try:
  if not select.select([read],[],[],5)[0]:raise RuntimeError('private display did not start')
  number=os.read(read,64).decode().strip();os.close(read)
  env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
  for key in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):env.pop(key,None)
  # Leak checking is exercised separately without ptrace.
  env['ASAN_OPTIONS']=env.get('ASAN_OPTIONS','')+':detect_leaks=0'
  def run(*args):return subprocess.run(args,env=env,check=True,capture_output=True,text=True,timeout=5)
  conditions=[
   ('tfx::cockpit::hud::draw','state.chaff == 15 && state.flares == 15 && !state.stealth && !state.ecm','INITIAL'),
   ('tfx::cockpit::hud::draw','state.chaff == 14 && state.flares == 15','CHAFF_HUD'),
   ('tfx::cockpit::hud::draw','state.chaff == 14 && state.flares == 14','FLARE_HUD'),
   ('tfx::cockpit::hud::draw','state.ecm && !state.stealth','ECM_HUD'),
   ('tfx::weapons::advance_motion','context.player_decoys','ECM_MOTION'),
   ('tfx::cockpit::hud::draw','state.stealth && !state.ecm && state.chaff == 14 && state.flares == 14','STEALTH_HUD'),
   ('tfx::sensors::advance_air_lock','context.sensor_suppressed','STEALTH_SENSOR'),
   ('tfx::weapons::runtime::countermeasure','owner.pool == tfx::weapons::actor_pool::player && radar && strength == 60','CHAFF_RELEASE'),
   ('tfx::weapons::runtime::countermeasure','owner.pool == tfx::weapons::actor_pool::player && !radar && strength == 40','FLARE_RELEASE'),
  ]
  script=['set pagination off','set confirm off','set startup-with-shell off','set print thread-events off']
  for index,(function,condition,label) in enumerate(conditions,1):
   script += [f'break {function} if {condition}','commands','silent',f'printf "TFX_DEFENCE_{label}\\n"',f'disable {index}','continue','end']
  debugger=directory/'observe.gdb';debugger.write_text('\n'.join(script+['run'])+'\n')
  log=directory/'game.log'
  with log.open('w') as output:
   game=subprocess.Popen(['gdb','--quiet','--batch','-x',str(debugger),'--args',str((a.build/'tfx').resolve()),'--data-dir',str(install),'--mission','512','--scale','2'],env=env,stdout=output,stderr=subprocess.STDOUT)
   def await_text(label,timeout=30):
    wanted='TFX_DEFENCE_'+label;deadline=time.monotonic()+timeout
    while time.monotonic()<deadline and game.poll() is None:
     if wanted in log.read_text():return
     time.sleep(.05)
    raise RuntimeError('missing '+wanted+':\n'+log.read_text()[-6000:])
   deadline=time.monotonic()+30;window=None
   while time.monotonic()<deadline and game.poll() is None:
    windows=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True).stdout.splitlines()
    if windows:window=windows[0];break
    time.sleep(.05)
   if not window:raise RuntimeError(log.read_text()[-6000:])
   run('xdotool','windowfocus','--sync',window)
   def tap(key):run('xdotool','keydown',key);time.sleep(.2);run('xdotool','keyup',key);time.sleep(.2)
   await_text('INITIAL');tap('s');await_text('STEALTH_SENSOR')
   tap('Insert');await_text('CHAFF_RELEASE');await_text('CHAFF_HUD')
   tap('Delete');await_text('FLARE_RELEASE');await_text('FLARE_HUD')
   tap('e');await_text('ECM_HUD');await_text('ECM_MOTION')
   tap('s');await_text('STEALTH_HUD')
   tap('Shift_L+q');confirm_flight_quit(env,window);game.wait(timeout=15)
   text=log.read_text()
   assert game.returncode==0 and 'exited normally' in text and 'ERROR:' not in text and 'Sanitizer' not in text,text[-6000:]
   print('Private silent X11 defensive controls pass: real Insert/Delete produce player-owned decoys and changing HUD counts; E updates HUD and motion ECM context; S restores stealth and the sensor gate; clean exit')
 finally:
  for process in (game,display):
   if process is not None and process.poll() is None:
    process.terminate()
    try:process.wait(timeout=3)
    except subprocess.TimeoutExpired:process.kill();process.wait()
