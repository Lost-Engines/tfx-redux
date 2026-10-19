#!/usr/bin/env python3
"""Observe config/CLI selection, live autopilot, replay, mouse and auto landing.

A private installation uses symlinks to the original archives and a temporary
configuration. GDB observes real callbacks without changing game state. X11
input alone operates the game, so the original installation is never modified.
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
a=p.parse_args();executable=(a.build/'tfx').resolve();data=a.data_dir.resolve()
for override in (False,True):
 with tempfile.TemporaryDirectory(prefix='tfx-x11-autopilot-') as temporary:
  directory=Path(temporary);install=directory/'TFX';install.mkdir()
  for name in ('DID.DAT','TFX.DAT'):(install/name).symlink_to(data/name)
  (install/'TFX.CFG').write_text('CONTROL=0\r\nMODEL=2\r\nHUDCOL=111\r\nREALISM=0,1,1,1,0,0,0\r\nJOYSCALE=512\r\n')
  read,write=os.pipe();display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write);game=None
  try:
   if not select.select([read],[],[],5)[0]:raise RuntimeError('private display did not start')
   number=os.read(read,64).decode().strip();os.close(read)
   env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
   for key in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):env.pop(key,None)
   def run(*command):return subprocess.run(command,env=env,timeout=5,check=True,capture_output=True,text=True)
   def tap(key):
    run('xdotool','keydown',key);time.sleep(.25);run('xdotool','keyup',key);time.sleep(.2)
   debugger=directory/'observe.gdb'
   model='military_spec' if override else 'realistic';source='mouse' if override else 'keyboard'
   script=['set pagination off','set confirm off','set startup-with-shell off','set print thread-events off','set $tfx_guided = 0']
   conditions=[
    ('tfx::flight::advance_aerodynamic_flight',f'state.model == tfx::simulation::flight_model::{model} && context.input.source == tfx::flight::control_source::{source}','CONFIG'),
    ('tfx::flight::advance_aerodynamic_flight','state.manual.controls.autopilot.enabled && $tfx_guided == 0','ENGAGED'),
    ('tfx::flight::advance_aerodynamic_flight','!state.manual.controls.autopilot.enabled && $tfx_guided == 1','DISENGAGED'),
    ('tfx::cockpit::radio_ticker::replay','1','REPLAY'),
    ('tfx::flight::advance_automatic_landing','landing.phase == tfx::flight::automatic_landing_phase::prepare','LANDING'),
   ]
   if override:conditions.append(('tfx::flight::update_aerodynamic_controls','context.source == tfx::flight::control_source::mouse && (context.mouse.x != 0 || context.mouse.y != 0)','MOUSE'))
   for index,(function,condition,label) in enumerate(conditions,start=1):
    script.extend([f'break {function} if {condition}','commands','silent',f'printf "TFX_WINDOW_{label}\\n"'])
    if label=='ENGAGED':script.append('set $tfx_guided = 1')
    if label=='DISENGAGED':script.append('set $tfx_guided = 2')
    script.extend([f'disable {index}','continue','end'])
   debugger.write_text('\n'.join(script+['run'])+'\n')
   with (directory/'game.log').open('w+') as log:
    def output():log.flush();return (directory/'game.log').read_text()
    def await_text(text,timeout=25):
     deadline=time.monotonic()+timeout
     while time.monotonic()<deadline and game.poll() is None:
      if text in output():return
      time.sleep(.05)
     raise RuntimeError('missing '+text+':\n'+output()[-6000:])
    args=[str(executable),'--data-dir',str(install),'--mission','513','--scale','2']
    if override:args+=['--flight-model','military-spec','--control','mouse']
    game=subprocess.Popen(['gdb','--quiet','--batch','-x',str(debugger),'--args',*args],env=env,stdout=log,stderr=subprocess.STDOUT)
    deadline=time.monotonic()+20;window=None
    while time.monotonic()<deadline and game.poll() is None:
     found=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True)
     if found.stdout.strip():window=found.stdout.splitlines()[0];break
     time.sleep(.05)
    if window is None:raise RuntimeError('window did not appear:\n'+output()[-5000:])
    run('xdotool','windowfocus','--sync',window);await_text('TFX_WINDOW_CONFIG')
    tap('8');tap('a');await_text('TFX_WINDOW_ENGAGED')
    tap('a');await_text('TFX_WINDOW_DISENGAGED')
    if override:
     run('xdotool','mousemove_relative','--','120','60');await_text('TFX_WINDOW_MOUSE')
     run('xdotool','mousemove_relative','--','-120','-60')
    tap('l');await_text('TFX_WINDOW_REPLAY')
    tap('i');tap('Alt_L+l');await_text('TFX_WINDOW_LANDING')
    tap('Shift_L+q');confirm_flight_quit(env,window);game.wait(timeout=10)
    assert game.returncode==0 and 'exited normally' in output() and 'Program received signal' not in output(),output()[-5000:]
    print(f'Private X11 {"CLI override/mouse" if override else "configuration/keyboard"} passed: selected model/source, heading hold, A on/off, plain L replay, I then Alt+L reaches scripted landing, clean quit')
  finally:
   for process in (game,display):
    if process is not None and process.poll() is None:
     process.terminate()
     try:process.wait(timeout=3)
     except subprocess.TimeoutExpired:process.kill();process.wait()
