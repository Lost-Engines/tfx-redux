#!/usr/bin/env python3
"""Observe live native texture drawing from the original game atlases.

Uses private silent installs/X11 and observation-only GDB. A real F10 canopy
view is selected; source, texture contents, poses and results are not changed.
"""
import argparse
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time
from window_input import confirm_flight_quit
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,default=Path('build'));p.add_argument('--data-dir',type=Path,required=True);p.add_argument('--mission',default='501');a=p.parse_args()
source=Path('src/render/integer_polygon.cpp').resolve();line=next(i for i,text in enumerate(source.read_text().splitlines(),1) if 'uint8_t const ink{remap[index < texture.size()' in text)
with tempfile.TemporaryDirectory(prefix='tfx-texture-window-') as temporary:
 directory=Path(temporary);install=directory/'TFX';install.mkdir()
 for name in ('DID.DAT','TFX.DAT'):(install/name).symlink_to((a.data_dir/name).resolve())
 (install/'TFX.CFG').write_bytes(b'CONTROL=0\r\nMODEL=0\r\nSOUNDFX=0\r\nMUSIC=0\r\nREALISM=0,0,1,1,0,0,0\r\n')
 read,write=os.pipe();display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write);game=None
 try:
  if not select.select([read],[],[],5)[0]:raise RuntimeError('private X11 display did not start')
  number=os.read(read,64).decode().strip();os.close(read);env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
  for key in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):env.pop(key,None)
  env['ASAN_OPTIONS']=env.get('ASAN_OPTIONS','')+':detect_leaks=0' # LSan cannot run under ptrace.
  script=directory/'observe.gdb';script.write_text(f'''set pagination off
set confirm off
set startup-with-shell off
set print thread-events off
break {source}:{line}
commands
silent
printf "TFX_NATIVE_TEXTURE_DRAW %ld\\n", texture._M_extent._M_extent_value
disable 1
continue
end
run
''')
  log=directory/'game.log'
  with log.open('w') as stream:
   game=subprocess.Popen(['gdb','--quiet','--batch','-x',str(script),'--args',str((a.build/'tfx').resolve()),'--data-dir',str(install),'--mission',a.mission,'--scale','2'],env=env,stdout=stream,stderr=subprocess.STDOUT)
   def run(*command):return subprocess.run(command,env=env,check=True,capture_output=True,text=True,timeout=5)
   deadline=time.monotonic()+30;window=None
   while time.monotonic()<deadline and game.poll() is None:
    windows=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True).stdout.splitlines()
    if windows:window=windows[0];break
    time.sleep(.05)
   if not window:raise RuntimeError('no flight window:\n'+log.read_text()[-6000:])
   run('xdotool','windowfocus','--sync',window);run('xdotool','keydown','F10');time.sleep(.15);run('xdotool','keyup','F10')
   deadline=time.monotonic()+15
   while time.monotonic()<deadline and game.poll() is None:
    if 'TFX_NATIVE_TEXTURE_DRAW 49152' in log.read_text():break
    time.sleep(.05)
   else:raise RuntimeError('canopy view did not execute native texture drawing:\n'+log.read_text()[-6000:])
   run('xdotool','keydown','Shift_L+q');time.sleep(.15);run('xdotool','keyup','Shift_L+q');confirm_flight_quit(env,window);game.wait(timeout=15)
   text=log.read_text();assert game.returncode==0 and 'exited normally' in text and 'ERROR:' not in text and 'Sanitizer' not in text,text[-6000:]
   print('Private X11 native texture integration passed: original retail atlases, F10 canopy view, live integer texture drawing and clean exit; no world-state mutation')
 finally:
  for process in (game,display):
   if process is not None and process.poll() is None:
    process.terminate()
    try:process.wait(timeout=3)
    except subprocess.TimeoutExpired:process.kill();process.wait()
