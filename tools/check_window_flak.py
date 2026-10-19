#!/usr/bin/env python3
"""Seed private flak or sunlight-glare inputs and observe their live consumers.

This explicitly supplies a height, small region and local flak RNG state in the
private process. The complete controller/pool has separate native comparisons;
this checks the actual model, scene and daytime palette-flash handoff.
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
p.add_argument('--mission')
p.add_argument('--night',action='store_true')
p.add_argument('--sun',action='store_true',help='supply a centred sun-facing projection instead of a flak region')
a=p.parse_args();a.mission=a.mission or ('508' if a.night else '503')
with tempfile.TemporaryDirectory(prefix='tfx-flak-') as temporary:
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
  model=4 if a.night else 0
  script=['set pagination off','set confirm off','set startup-with-shell off','set print thread-events off','set $spawned = 0',
   'break tfx::world::advance_flak','commands','silent',
   'set state.delay = 0','set state.random.phase = 0','set state.random.scatter = 1',
   f'set state.night_model = {model}','set context.player.y = -700','set context.cloud_layer = false',
   'set context.regions._M_ptr[0].first.x = context.player.x - 100',
   'set context.regions._M_ptr[0].first.y = context.player.z - 100',
   'set context.regions._M_ptr[0].last.x = context.player.x + 100',
   'set context.regions._M_ptr[0].last.y = context.player.z + 100',
   'printf "TFX_FLAK_SEEDED\\n"','disable 1','continue','end',
   f'break tfx::world::flak_pool::spawn if event.model == {model}','commands','silent',
   'set $spawned = 1','printf "TFX_FLAK_SPAWNED\\n"','disable 2','continue','end',
   'break tfx::world::effect_position if $spawned','commands','silent',
   'printf "TFX_FLAK_POSITIONED\\n"','disable 3','continue','end',
   'break tfx::world::scene::draw if $spawned','commands','silent',
   'printf "TFX_FLAK_DRAWN\\n"','disable 4','continue','end']
  if not a.night:
   script += ['break tfx::render::palette_lighting::flash if level == 40 && $spawned','commands','silent',
    'printf "TFX_FLAK_FLASH\\n"','disable 5','continue','end',
    'break tfx::cockpit::cockpit_position::shake','commands','silent',
    'printf "TFX_FLAK_SHAKEN\\n"','disable 6','continue','end']
  if a.sun:
   script=['set pagination off','set confirm off','set startup-with-shell off','set print thread-events off','set $flashed = 0',
    'break tfx::render::glare_level','commands','silent',
    'set context.camera_anchor.x = 0','set context.camera_anchor.y = 0','set context.camera_anchor.z = 0',
    'set context.cloud_layer = false','set context.flak_flash = false',
    'set context.view = tfx::cockpit::view_mode::cockpit',
    'set projection.position_q16.x = 0','set projection.position_q16.y = 0','set projection.position_q16.z = 0']
   for index,value in enumerate((32767,0,0,0,-32767,32767,0,-32767,-32767)):
    script.append(f'set projection.matrix.data[{index}] = {value}')
   script += ['printf "TFX_FLAK_SEEDED\\n"','disable 1','continue','end',
    'break tfx::render::palette_lighting::flash if level == 1','commands','silent',
    'set $flashed = 1','printf "TFX_FLAK_FLASH\\n"','disable 2','continue','end',
    'break tfx::render::palette_lighting::advance if $flashed','commands','silent',
    'printf "TFX_FLAK_PAINTED\\n"','disable 3','continue','end',
    'break tfx::world::scene::draw if $flashed','commands','silent',
    'printf "TFX_FLAK_DRAWN\\n"','disable 4','continue','end',
    'break tfx::weapons::runtime::advance','commands','silent',
    'printf "TFX_FLAK_READY\\n"','disable 5','continue','end']
  debugger=directory/'observe.gdb';debugger.write_text('\n'.join(script+['run'])+'\n');log=directory/'game.log'
  with log.open('w') as output:
   game=subprocess.Popen(['gdb','--quiet','--batch','-x',str(debugger),'--args',str((a.build/'tfx').resolve()),'--data-dir',str(install),'--mission',a.mission,'--scale','2'],env=env,stdout=output,stderr=subprocess.STDOUT)
   def await_text(label,timeout=45):
    marker='TFX_FLAK_'+label;deadline=time.monotonic()+timeout
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
   labels=('SEEDED','FLASH','PAINTED','DRAWN') if a.sun else ('SEEDED','SPAWNED','POSITIONED','DRAWN') if a.night else ('SEEDED','SPAWNED','POSITIONED','DRAWN','FLASH','SHAKEN')
   for label in labels:await_text(label)
   if a.sun:await_text('READY')
   run('xdotool','keydown','Shift_L+q')
   try:confirm_flight_quit(env,window);game.wait(timeout=15)
   except subprocess.TimeoutExpired as error:raise RuntimeError('exit timed out:\n'+log.read_text()[-6000:]) from error
   finally:run('xdotool','keyup','Shift_L+q')
   text=log.read_text();assert game.returncode==0 and 'exited normally' in text and 'ERROR:' not in text and 'Sanitizer' not in text,text[-6000:]
   if a.sun:print('Private sunlight-glare handoff passes: explicitly supplied centred projection, real selector, strongest glare level, palette painter, world drawing and clean exit')
   else:print(('Private night flak' if a.night else 'Private day flak') + ' handoff passes: explicitly supplied region/height/RNG, real controller, correct model pool, word placement, scene drawing' + ('' if a.night else ', palette flash, cockpit shake') + ' and clean exit')
 finally:
  for process in (game,display):
   if process is not None and process.poll() is None:
    process.terminate()
    try:process.wait(timeout=3)
    except subprocess.TimeoutExpired:process.kill();process.wait()
