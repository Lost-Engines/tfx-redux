#!/usr/bin/env python3
"""Observe real designator entry, zoom, tracking and cockpit return.

Private silent installation/display; GDB only observes actual camera/display
arguments, without changing player, target or controller state. The complete
original controller and pixel comparison is check_designator.py.
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
with tempfile.TemporaryDirectory(prefix='tfx-designator-') as temporary:
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
  def tap(key):run('xdotool','keydown',key);time.sleep(.15);run('xdotool','keyup',key);time.sleep(.15)
  conditions=[
   ('tfx::cockpit::designator_pose','state.zoom == 0','SEARCH_CAMERA'),
   ('tfx::cockpit::designator_pose','state.zoom > 0','ZOOM_CAMERA'),
   ('tfx::cockpit::designator_display::draw','!tracking','SEARCH_DISPLAY'),
   ('tfx::cockpit::designator_display::draw','tracking','TRACKING_DISPLAY'),
   ('tfx::cockpit::view_camera::advance','views.state.current == tfx::cockpit::view_mode::ejection','EJECTION_CAMERA'),
  ]
  script=['set pagination off','set confirm off','set startup-with-shell off','set print thread-events off']
  for index,(function,condition,label) in enumerate(conditions,1):script += [f'break {function} if {condition}','commands','silent',f'printf "TFX_DESIGNATOR_{label}\\n"',f'disable {index}','continue','end']
  debugger=directory/'observe.gdb';debugger.write_text('\n'.join(script+['run'])+'\n');log=directory/'game.log'
  with log.open('w') as output:
   game=subprocess.Popen(['gdb','--quiet','--batch','-x',str(debugger),'--args',str((a.build/'tfx').resolve()),'--data-dir',str(install),'--mission','601','--control','keyboard','--flight-model','arcade','--scale','2'],env=env,stdout=output,stderr=subprocess.STDOUT)
   def await_text(label,timeout=30):
    marker='TFX_DESIGNATOR_'+label;deadline=time.monotonic()+timeout
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
   run('xdotool','windowfocus','--sync',window);time.sleep(.3)
   tap('o');tap('z')
   await_text('SEARCH_CAMERA');await_text('SEARCH_DISPLAY')
   tap('period');await_text('ZOOM_CAMERA')
   tap('Right');tap('z');await_text('TRACKING_DISPLAY')
   tap('x');tap('F1');tap('p')
   run('import','-window',window,str(directory/'returned.png'))
   from PIL import Image
   image=Image.open(directory/'returned.png').convert('RGB').resize((320,200),Image.Resampling.NEAREST)
   # Returning to F1 restores actual cockpit pixels below the world viewport.
   assert len(image.crop((35,175,285,199)).getcolors(maxcolors=64000))>8,'cockpit did not return after leaving designator'
   tap('p');tap('z');tap('Escape');tap('Escape');await_text('EJECTION_CAMERA')
   tap('Shift_L+q');confirm_flight_quit(env,window);game.wait(timeout=15)
   text=log.read_text();assert game.returncode==0 and 'exited normally' in text and 'ERROR:' not in text and 'Sanitizer' not in text,text[-6000:]
   print('Private silent live designator passes: normal weapon selection and keys reach native search/zoom camera, authored search/tracking overlays, unlock, cockpit return, pause, ejection from designator and clean exit; no scenario state was injected')
 finally:
  for process in (game,display):
   if process is not None and process.poll() is None:
    process.terminate()
    try:process.wait(timeout=3)
    except subprocess.TimeoutExpired:process.kill();process.wait()
