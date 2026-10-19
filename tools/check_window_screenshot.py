#!/usr/bin/env python3
"""Check real 4-key captures against the paused window in normal/infrared flight.

Uses a private display and silent disposable installation. Existing numbered
captures must remain untouched. No original installation files are modified.
"""
import argparse
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time
from PIL import Image
from ilbm import decode_ilbm
from window_input import confirm_flight_quit

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--research',type=Path,default=Path('../tfx-reverse'))
p.add_argument('--build',type=Path,default=Path('build'))
a=p.parse_args();data=a.research.resolve()/'installs/cd_1994_09/TFX'
with tempfile.TemporaryDirectory(prefix='tfx-screenshot-window-')as temporary:
 root=Path(temporary);install=root/'TFX';install.mkdir()
 for name in ('TFX.DAT','DID.DAT'):(install/name).symlink_to(data/name)
 original=b'CONTROL=0\r\nMODEL=0\r\nSOUNDFX=0\r\nMUSIC=0\r\n'
 config=install/'TFX.CFG';config.write_bytes(original)
 protected=install/'GRAB_00.LBM';protected.write_bytes(b'Existing user capture')
 read,write=os.pipe();display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write);game=None
 try:
  if not select.select([read],[],[],5)[0]:raise RuntimeError('private display did not start')
  number=os.read(read,64).decode().strip();os.close(read)
  env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
  for name in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):env.pop(name,None)
  def run(*command):return subprocess.run(command,env=env,check=True,capture_output=True,text=True,timeout=5)
  def tap(key):run('xdotool','keydown',key);time.sleep(.12);run('xdotool','keyup',key);time.sleep(.2)
  with (root/'game.log').open('w')as log:
   game=subprocess.Popen([str((a.build/'tfx').resolve()),'--data-dir',str(install),'--mission','501','--environment','yugos_n','--scale','2'],env=env,stdout=log,stderr=subprocess.STDOUT)
   deadline=time.monotonic()+20;window=None
   while time.monotonic()<deadline and game.poll()is None:
    found=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True)
    if found.stdout.strip():window=found.stdout.splitlines()[0];break
    time.sleep(.05)
   assert window is not None,(root/'game.log').read_text()
   run('xdotool','windowfocus','--sync',window);time.sleep(.5)
   def check_capture(number):
    screen=root/'window.png';run('import','-window',window,str(screen))
    with Image.open(screen)as image:expected=image.convert('RGB').resize((320,200),Image.Resampling.NEAREST).tobytes()
    tap('4');path=install/f'GRAB_{number:02}.LBM'
    assert path.is_file(),('missing capture',path,(root/'game.log').read_text())
    size,indices,palette=decode_ilbm(path.read_bytes());assert size==(320,200),size
    actual=bytes(((palette[index*3+channel]>>2)*255+31)//63 for index in indices for channel in range(3))
    assert actual==expected,('capture differs from the paused displayed image',number,sum(x!=y for x,y in zip(actual,expected)))
    assert protected.read_bytes()==b'Existing user capture','existing screenshot was overwritten'
    return path.read_bytes()
   tap('p');first=check_capture(1);assert check_capture(2)==first,'paused captures changed'
   tap('p');tap('Shift_L+n');tap('p');infrared=check_capture(3)
   assert infrared[48:816]!=first[48:816],'infrared capture kept the normal palette'
   tap('Shift_L+q');confirm_flight_quit(env,window);game.wait(timeout=15)
   text=(root/'game.log').read_text()
   assert game.returncode==0 and 'ERROR:'not in text and 'Sanitizer'not in text,text[-5000:]
   assert config.read_bytes()==original,'capturing or quitting rewrote preferences'
   print('Private X11 screenshot key passed: numbered ILBM files, existing-file preservation, exact paused window pixels and active infrared palette')
 finally:
  for process in (game,display):
   if process is not None and process.poll()is None:
    process.terminate()
    try:process.wait(timeout=3)
    except subprocess.TimeoutExpired:process.kill();process.wait()
