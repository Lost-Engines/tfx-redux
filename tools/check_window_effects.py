#!/usr/bin/env python3
"""Seed one aircraft ground impact and observe its live effect consumers.

A private silent process receives an explicitly controlled falling-aircraft
state through GDB. The original pool/SSD arithmetic has separate comparisons;
this check establishes the actor-to-pool-to-scene handoff and clean rendering.
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
p.add_argument('--trails',action='store_true',help='seed a nearby missile and observe its live smoke trail')
p.add_argument('--projectile',action='store_true',help='seed a cannon ground impact and supply its already compared terrain classification')
a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='tfx-effects-') as temporary:
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
  script=['set pagination off','set confirm off','set startup-with-shell off','set print thread-events off','set $spawned = 0',
   'break tfx::actors::air_runtime::advance_actor if index == 0','commands','silent',
   'set this->actors._M_elems[index]._M_payload._M_payload._M_value.actions = 3',
   'set this->actors._M_elems[index]._M_payload._M_payload._M_value.flight.input_ticks = 0',
   'set this->actors._M_elems[index]._M_payload._M_payload._M_value.flight.pose.position.y = 0',
   'printf "TFX_EFFECTS_SEEDED\\n"','disable 1','continue','end',
   'break tfx::world::effect_pool::spawn','commands','silent','set $spawned = 1',
   'printf "TFX_EFFECTS_SPAWNED\\n"','disable 2','continue','end',
   'break tfx::world::effect_position if $spawned','commands','silent',
   'printf "TFX_EFFECTS_POSITIONED\\n"','disable 3','continue','end',
   'break tfx::world::scene::draw if $spawned','commands','silent',
   'printf "TFX_EFFECTS_DRAWN\\n"','disable 4','continue','end']
  if a.projectile:
   script=['set pagination off','set confirm off','set startup-with-shell off','set print thread-events off','set $spawned = 0','set $classify = 0',
    'break tfx::weapons::runtime::advance if this->first != 0','commands','silent',
    'set this->first->value.state.position.y = 0','set this->first->value.state.speed = 0',
    'set this->first->value.state.weapon = 0','set this->first->value.state.ticks = 100',
    'set this->first->value.state.motion = tfx::weapons::flight_stage::falling_store',
    'set $classify = 1','printf "TFX_EFFECTS_SEEDED\\n"','disable 1','continue','end',
    'break tfx::world::classify_impact if $classify','commands','silent',
    'printf "TFX_EFFECTS_CLASSIFIED\\n"','disable 2','return (tfx::world::impact_surface)0','continue','end',
    'break tfx::world::effect_pool::spawn if kind == tfx::world::effect_kind::cannon_hit','commands','silent',
    'set $spawned = 1','printf "TFX_EFFECTS_SPAWNED\\n"','disable 3','continue','end',
    'break tfx::world::effect_position if $spawned','commands','silent',
    'printf "TFX_EFFECTS_POSITIONED\\n"','disable 4','continue','end',
    'break tfx::world::scene::draw if $spawned','commands','silent',
    'printf "TFX_EFFECTS_DRAWN\\n"','disable 5','continue','end',
    'break tfx::weapons::runtime::retire if slot.value.spent && slot.value.state.weapon == 0','commands','silent',
    'printf "TFX_EFFECTS_RETIRED\\n"','disable 6','continue','end',
    'break tfx::weapons::runtime::advance','commands','silent',
    'printf "TFX_EFFECTS_READY\\n"','disable 7','continue','end']
  if a.trails:
   script=['set pagination off','set confirm off','set startup-with-shell off','set print thread-events off','set $spawned = 0',
    'break tfx::weapons::runtime::advance if this->first != 0','commands','silent',
    'set this->first->value.state.weapon = 1','set this->first->value.state.speed = 2048',
    'set this->first->value.state.range = 1000000','set this->first->value.state.ticks = 100',
    'set this->first->value.state.motion = tfx::weapons::flight_stage::coasting',
    'printf "TFX_EFFECTS_SEEDED\\n"','disable 1','continue','end',
    'break tfx::world::trail_pool::spawn','commands','silent','set $spawned = 1',
    'printf "TFX_EFFECTS_SPAWNED\\n"','disable 2','continue','end',
    'break tfx::world::trail_position if $spawned','commands','silent',
    'printf "TFX_EFFECTS_POSITIONED\\n"','disable 3','continue','end',
    'break tfx::world::scene::draw if $spawned','commands','silent',
    'printf "TFX_EFFECTS_DRAWN\\n"','disable 4','continue','end',
    'break tfx::weapons::runtime::advance','commands','silent',
    'printf "TFX_EFFECTS_READY\\n"','disable 5','continue','end']
  debugger=directory/'observe.gdb';debugger.write_text('\n'.join(script+['run'])+'\n');log=directory/'game.log'
  with log.open('w') as output:
   game=subprocess.Popen(['gdb','--quiet','--batch','-x',str(debugger),'--args',str((a.build/'tfx').resolve()),'--data-dir',str(install),'--mission',a.mission,'--scale','2'],env=env,stdout=output,stderr=subprocess.STDOUT)
   def await_text(label,timeout=45):
    marker='TFX_EFFECTS_'+label;deadline=time.monotonic()+timeout
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
   if a.projectile or a.trails:
    await_text('READY')
    run('xdotool','keydown','space');time.sleep(.15);run('xdotool','keyup','space')
   for label in (('SEEDED','CLASSIFIED','SPAWNED','POSITIONED','DRAWN','RETIRED') if a.projectile else ('SEEDED','SPAWNED','POSITIONED','DRAWN')):await_text(label)
   run('xdotool','keydown','Shift_L+q')
   try:confirm_flight_quit(env,window);game.wait(timeout=15)
   except subprocess.TimeoutExpired as error:raise RuntimeError('exit timed out:\n'+log.read_text()[-6000:]) from error
   finally:run('xdotool','keyup','Shift_L+q')
   text=log.read_text();assert game.returncode==0 and 'exited normally' in text and 'ERROR:' not in text and 'Sanitizer' not in text,text[-6000:]
   if a.trails:print('Private silent missile-trail handoff passes: explicitly supplied missile, real dispatch and visibility gates, smoke pool, fractional placement, world drawing and clean exit')
   else:print(('Private silent controlled cannon impact and retirement pass' if a.projectile else 'Private silent aircraft impact pass') + ': explicitly supplied scenario, live effect request, pool spawn, word placement, world drawing and clean exit')
 finally:
  for process in (game,display):
   if process is not None and process.poll() is None:
    process.terminate()
    try:process.wait(timeout=3)
    except subprocess.TimeoutExpired:process.kill();process.wait()
