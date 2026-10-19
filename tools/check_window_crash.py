#!/usr/bin/env python3
"""Exercise land/water and solid-contact crash handoffs in a private silent window.

Unlike observer-only input checks, this deliberately seeds one collision at the
loss boundary through GDB. It changes only the private process's test scenario;
original selector/state arithmetic is established by check_impact/check_death.
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
p.add_argument('--screenshot-dir',type=Path)
p.add_argument('--observe-fixed-palette',action='store_true',help='observe the contact effect fixed palette during deterministic pre-window flight frames')
a=p.parse_args()
scene_source=Path('src/world/scene.cpp').resolve();placement_line=next(i for i,text in enumerate(scene_source.read_text().splitlines(),1) if 'actor.position = position;' in text)
for label,mission,collision in [('land','501',2),('water','504',1),('contact','602',0)]:
 with tempfile.TemporaryDirectory(prefix='tfx-crash-'+label+'-') as temporary:
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
   script=['set pagination off','set confirm off','set startup-with-shell off','set print thread-events off','set $selected = 0',
    'break tfx::flight::advance_arcade','commands','silent','set $flight = &state','disable 1','continue','end',
    'break tfx::missions::progress::observe_player_loss','commands','silent',
    f'set $flight->collision = {collision}','set $flight->position.y = 0','set $flight->integer_position.y = 0',
    'set context.collision = true','set context.height = 0','printf "TFX_CRASH_SEEDED\\n"','disable 2','continue','end',
    f'break tfx::world::select_crash_appearance if collision == {collision}'+(' && water_world' if label=='water' else ''),
    'commands','silent','set $selected = 1','printf "TFX_CRASH_SELECTED\\n"','disable 3','continue','end',
    f'break {scene_source}:{placement_line} if $selected && angles.x == 0 && angles.y == 0 && angles.z == 0',
    'commands','silent','printf "TFX_CRASH_PLACED\\n"','disable 4','continue','end',
    'break tfx::cockpit::view_camera::advance if views.state.current == tfx::cockpit::view_mode::impact_orbit && context.impact_target_q8._M_payload._M_engaged',
    'commands','silent','printf "TFX_CRASH_CAMERA\\n"','disable 5','continue','end','run']
   if label=='contact':
    script=['set pagination off','set confirm off','set startup-with-shell off','set print thread-events off','set $selected = 0',
     'break tfx::world::resolve_player_contact','commands','silent',
     'set context.destroyed = true','set context.previously_destroyed = false',
     'printf "TFX_CRASH_SEEDED\\n"','disable 1','continue','end',
     'break tfx::missions::progress::begin_contact_explosion','commands','silent',
     'set $selected = 1','printf "TFX_CRASH_SELECTED\\n"','disable 2','continue','end',
     'break tfx::world::scene::place_actor if $selected','commands','silent',
     'printf "TFX_CRASH_PLACED\\n"','disable 3','continue','end',
     'break tfx::cockpit::view_camera::advance if views.state.current == tfx::cockpit::view_mode::unbanked_chase',
     'commands','silent','printf "TFX_CRASH_CAMERA\\n"','disable 4','continue','end','run']
   if a.observe_fixed_palette and label=='contact':
    # The contact model (kapow) explicitly selects the fixed table; the land
    # crash model does not. Observe its actual renderer boundary.
    # Optimisation can split the world's conditional-remap call across source
    # locations. Observe the actual renderer boundary instead of one line map.
    script[-1:-1]=['break tfx::render::software_renderer::primitive if source.palette == tfx::models::palette_source::fixed','commands','silent','printf "TFX_FIXED_PALETTE_DRAW\\n"','disable $bpnum','continue','end']
    observer=sum(line.startswith('break ') for line in script)
    # A conditional painter breakpoint during startup can stop thousands of
    # times and advance the external flight clock before the seeded impact.
    # Arm it only once the selected crash camera is about to draw its frame.
    camera=script.index('printf "TFX_CRASH_CAMERA\\n"')
    script[camera+1:camera+1]=[f'enable {observer}']
    script[-1:-1]=[f'disable {observer}']
   if label=='water':
    # Put the controlled sea impact clear of the starting carrier, so the splash is visible.
    at=script.index('set context.collision = true')
    script[at:at]=['set $flight->position.x = $flight->position.x + 3000','set $flight->integer_position.x = (int)$flight->position.x']
   debugger=directory/'scenario.gdb';debugger.write_text('\n'.join(script)+'\n');log=directory/'game.log'
   with log.open('w') as output:
    game=subprocess.Popen(['gdb','--quiet','--batch','-x',str(debugger),'--args',str((a.build/'tfx').resolve()),'--data-dir',str(install),'--mission',mission,'--scale','2']+(['--flight-frames','64'] if a.observe_fixed_palette and label=='contact' else []),env=env,stdout=output,stderr=subprocess.STDOUT)
    def await_text(marker,timeout=30):
     deadline=time.monotonic()+timeout
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
    for marker in ('SEEDED','SELECTED','PLACED','CAMERA'):await_text('TFX_CRASH_'+marker)
    if a.observe_fixed_palette and label=='contact':await_text('TFX_FIXED_PALETTE_DRAW')
    time.sleep(.7)
    def tap(key):run('xdotool','keydown',key);time.sleep(.15);run('xdotool','keyup',key);time.sleep(.15)
    tap('p')
    if a.screenshot_dir:
     a.screenshot_dir.mkdir(parents=True,exist_ok=True);run('import','-window',window,str((a.screenshot_dir/(label+'.png')).resolve()))
    tap('Shift_L+q');confirm_flight_quit(env,window);game.wait(timeout=15);text=log.read_text()
    assert game.returncode==0 and 'exited normally' in text and 'ERROR:' not in text and 'Sanitizer' not in text,text[-6000:]
    print(f'Private {label} crash scenario passes: seeded contact, original model selection, authored effect placement, selected loss camera, rendered animation and clean exit',flush=True)
  finally:
   for process in (game,display):
    if process is not None and process.poll() is None:
     process.terminate()
     try:process.wait(timeout=3)
     except subprocess.TimeoutExpired:process.kill();process.wait()
