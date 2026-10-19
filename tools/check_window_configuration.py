#!/usr/bin/env python3
"""Operate the live native configuration menus using private X11 input.

GDB observes frame calls and selected physics without modifying game state.
Only a temporary installation/configuration is written; original assets are linked.
"""
import argparse
import os
from pathlib import Path
import select
import struct
import subprocess
import tempfile
import time
from window_input import move_game_pointer,confirm_flight_quit

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,default=Path('build'))
p.add_argument('--data-dir',type=Path,required=True)
a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='tfx-x11-configuration-') as temporary:
 directory=Path(temporary);install=directory/'TFX';install.mkdir()
 for name in ('DID.DAT','TFX.DAT'):(install/name).symlink_to((a.data_dir/name).resolve())
 config=install/'TFX.CFG'
 config.write_bytes(b'CONTROL=0\r\nMODEL=0\r\nDETAIL=2\r\nHUDCOL=111\r\nREALISM=0,1,1,1,0,1,1\r\nJOYSCALE=512\r\nEXTRA=keep this\r\n')
 read,write=os.pipe();display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write);game=None
 try:
  if not select.select([read],[],[],5)[0]:raise RuntimeError('private display did not start')
  number=os.read(read,64).decode().strip();os.close(read)
  env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR=temporary,XMODIFIERS='@im=none')
  # LeakSanitizer cannot inspect a process under GDB/ptrace. Ordinary
  # sanitizer regressions still run with leak detection enabled.
  env['ASAN_OPTIONS']=env.get('ASAN_OPTIONS','')+':detect_leaks=0'
  for key in ('WAYLAND_DISPLAY','XDG_SESSION_TYPE','DBUS_SESSION_BUS_ADDRESS'):env.pop(key,None)
  def run(*args):return subprocess.run(args,env=env,check=True,capture_output=True,text=True,timeout=5)
  def tap(key):run('xdotool','keydown',key);time.sleep(.2);run('xdotool','keyup',key);time.sleep(.2)
  script=directory/'observe.gdb'
  script.write_text('set pagination off\nset confirm off\nset startup-with-shell off\nset print thread-events off\nset $tfx_military = 0\nbreak tfx::flight::player_flight::advance\ncommands\nsilent\nprintf "TFX_WINDOW_TICK\\n"\nif this->physical.model == tfx::simulation::flight_model::military_spec && $tfx_military == 0\nprintf "TFX_WINDOW_MILITARY\\n"\nset $tfx_military = 1\nend\ncontinue\nend\nrun\n')
  with (directory/'game.log').open('w+') as log:
   def output():log.flush();return (directory/'game.log').read_text()
   def await_text(text,timeout=20):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline and game.poll() is None:
     if text in output():return
     time.sleep(.05)
    raise RuntimeError('missing '+text+'\n'+output()[-5000:])
   game=subprocess.Popen(['gdb','--quiet','--batch','-x',str(script),'--args',str((a.build/'tfx').resolve()),'--data-dir',str(install),'--mission','513','--scale','2'],env=env,stdout=log,stderr=subprocess.STDOUT)
   await_text('TFX_WINDOW_TICK')
   found=run('xdotool','search','--onlyvisible','--name','TFX (C|Redux)').stdout.splitlines();assert found
   window=found[0];run('xdotool','windowfocus','--sync',window)
   def title():return run('xdotool','getwindowname',window).stdout
   def await_title(text):
    deadline=time.monotonic()+15
    while time.monotonic()<deadline and game.poll() is None:
     if text in title():return
     time.sleep(.05)
    raise RuntimeError('missing title '+text+'\n'+output()[-5000:])
   def click(x,y):
    move_game_pointer(env,window,x,y)
    run('xdotool','mousedown','1');time.sleep(.12);run('xdotool','mouseup','1');time.sleep(.15)
   tap('grave');await_title('Configuration')
   before=output().count('TFX_WINDOW_TICK');time.sleep(.4);assert output().count('TFX_WINDOW_TICK')==before,'simulation advanced while configuration was open'
   click(180,140)
   run('xdotool','key','--repeat','9','--delay','50','BackSpace')
   run('xdotool','type','--clearmodifiers','--delay','40','QUICK.REC')
   tap('Return')
   click(252,108) # Military-spec; restores startup keyboard control.
   click(140,76) # Increase joystick scale by ten.
   click(136,92) # Medium detail.
   click(228,172);await_title('Realism')
   click(188,60) # Realistic G effects.
   click(128,124) # Wind on.
   click(104,140) # Temperature off.
   click(152,172);await_text('TFX_WINDOW_MILITARY')
   wanted=[b'REALISM=2,1,1,1,1,0,1',b'DETAIL=1',b'JOYSCALE=522',b'HUDCOL=111',b'CONTROL=0',b'MODEL=0',b'EXTRA=keep this']
   for line in wanted:assert line in config.read_bytes().splitlines(),(line,config.read_bytes())
   assert output().count('TFX_WINDOW_TICK')>before,'simulation did not resume'
   tap('alt+v');time.sleep(.3);tap('alt+v')
   recording=(install/'QUICK.REC').read_bytes()
   frames,size,trailer=struct.unpack_from('<HII',recording)
   assert frames>1 and size==len(recording) and 10<=trailer<size,(frames,size,trailer)
   assert not (install/'video.rec').exists(),'edited recording name was ignored'
   tap('grave');await_title('Configuration');click(80,172);click(188,108);await_title('Configuration');click(152,172)
   tap('Shift_L+q');await_title('Quit')
   stopped=output().count('TFX_WINDOW_TICK');time.sleep(.3)
   assert output().count('TFX_WINDOW_TICK')==stopped,'simulation advanced while quit confirmation was open'
   click(188,108) # NO returns straight to flight from the shortcut.
   deadline=time.monotonic()+10
   while time.monotonic()<deadline and output().count('TFX_WINDOW_TICK')==stopped:time.sleep(.05)
   assert output().count('TFX_WINDOW_TICK')>stopped,'quit cancellation did not return directly to flight'
   tap('Shift_L+q');confirm_flight_quit(env,window);game.wait(timeout=10)
   assert game.returncode==0 and 'exited normally' in output() and 'Program received signal' not in output(),output()[-5000:]
   print('Private X11 configuration passed: paused simulation, real mouse selections, live Military-spec callback, all realism controls, native preference save, edited REC filename/case, live capture, quit cancellation, resume and clean exit')
 finally:
  for process in (game,display):
   if process is not None and process.poll() is None:
    process.terminate()
    try:process.wait(timeout=3)
    except subprocess.TimeoutExpired:process.kill();process.wait()
