#!/usr/bin/env python3
"""Check CD progression and real effects through live configuration switches.

A test-authored anti-phase stereo ramp makes CD position measurable while
ordinary mono Sound Blaster effects cancel in the difference channel. All
input, assets written, display and audio output belong to private test state.
"""
import argparse
import array
import json
import os
from pathlib import Path
import select
import statistics
import subprocess
import tempfile
import time
import wave
from window_input import move_game_pointer,confirm_flight_quit

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,default=Path('build'))
p.add_argument('--data-dir',type=Path,default=Path('../tfx-reverse/installs/cd_1994_09/TFX'))
a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='tfx-audio-menu-') as temporary:
 root=Path(temporary);install=root/'TFX';install.mkdir();tracks=root/'tracks';tracks.mkdir()
 for name in ('DID.DAT','TFX.DAT','START.EXE'):(install/name).symlink_to((a.data_dir/name).resolve())
 (install/'TFX.CFG').write_bytes(b'CONTROL=0\r\nMODEL=0\r\nSOUNDFX=1\r\nMUSIC=1\r\nREALISM=0,0,1,1,0,0,0\r\nDETAIL=2\r\n')
 rate=48000
 with wave.open(str(tracks/'track09.wav'),'wb') as file:
  file.setparams((2,2,rate,0,'NONE','not compressed'))
  for second in range(60):
   pcm=array.array('h')
   for frame in range(rate):
    value=round((.03+.005*(second+frame/rate))*32768);pcm.extend((value,-value))
   file.writeframes(pcm.tobytes())
 sink='tfx_menu_'+str(os.getpid());module=display=game=capture=None
 try:
  module=subprocess.check_output(['pactl','load-module','module-null-sink','sink_name='+sink,'rate=48000','channels=2'],text=True).strip()
  read,write=os.pipe();display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write)
  assert select.select([read],[],[],5)[0],'private display did not start'
  number=os.read(read,64).decode().strip();os.close(read)
  env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_SESSION_TYPE='x11',PULSE_SINK=sink)
  for key in ('WAYLAND_DISPLAY','DBUS_SESSION_BUS_ADDRESS'):env.pop(key,None)
  def run(*command):return subprocess.run(command,env=env,capture_output=True,text=True,check=True,timeout=5)
  pcm=root/'audio.f32';log=root/'game.log'
  def values(start=None):
   data=pcm.read_bytes();data=data[len(data)//8*8-12000*8:] if start is None else data[start:]
   data=data[:len(data)//8*8];result=array.array('f');result.frombytes(data)
   assert result and all(abs(value)<1.1 for value in result),('invalid/missing PCM',log.read_text())
   return result
  def position():
   data=values();return statistics.median((data[i]-data[i+1])/2 for i in range(0,len(data),2))
  with pcm.open('wb') as output,log.open('w') as logging:
   capture=subprocess.Popen(['parec','--device='+sink+'.monitor','--format=float32le','--rate=48000','--channels=2','--latency-msec=20'],env=env,stdout=output,stderr=subprocess.DEVNULL)
   game=subprocess.Popen([str((a.build/'tfx').resolve()),'--data-dir',str(install),'--mission','501','--music','cd','--cd-audio-dir',str(tracks),'--scale','2'],env=env,cwd=root,stdout=logging,stderr=subprocess.STDOUT)
   deadline=time.monotonic()+30;window=None
   while time.monotonic()<deadline and game.poll() is None:
    found=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True).stdout.splitlines()
    if found and 'PCM output:' in log.read_text():window=found[0];break
    time.sleep(.05)
   assert window,log.read_text()
   run('xdotool','windowfocus','--sync',window)
   def tap(key):
    run('xdotool','keydown',key);time.sleep(.12);run('xdotool','keyup',key);time.sleep(.2)
   def click(x,y):
    move_game_pointer(env,window,x,y);run('xdotool','mousedown','1');time.sleep(.12);run('xdotool','mouseup','1');time.sleep(.2)
   time.sleep(2);before=position();tap('grave');time.sleep(2);after=position()
   assert after>before+.008,('CD restarted or stalled while configuration remained open',before,after,log.read_text())
   click(104,140);time.sleep(1)
   assert abs(position())<1e-7,('music-off did not stop CD playback',position(),run('xdotool','getwindowname',window).stdout)
   click(128,140);time.sleep(.5);restarted=position();time.sleep(1.5);continued=position()
   assert .03<=restarted<.04 and continued>restarted+.006,('music-on did not restart and continue the selected track',restarted,continued)
   click(152,172);time.sleep(.4)
   start=pcm.stat().st_size;tap('b');time.sleep(2.8);mixed=values(start)
   assert max(abs((mixed[i]+mixed[i+1])/2) for i in range(0,len(mixed),2))>.1,('airbrake effect did not mix with CD music',log.read_text())
   tap('grave');click(104,124);click(152,172);time.sleep(2.8)
   start=pcm.stat().st_size;tap('b');time.sleep(2);muted=values(start)
   assert max(abs((muted[i]+muted[i+1])/2) for i in range(0,len(muted),2))<1e-6,'sound-off still accepted a new effect'
   assert position()>.04,'muting effects stopped or restarted the CD'
   tap('grave');click(128,124);click(152,172);time.sleep(.4)
   start=pcm.stat().st_size;tap('b');tap('f');tap('space');time.sleep(2);resumed=values(start)
   assert max(abs((resumed[i]+resumed[i+1])/2) for i in range(0,len(resumed),2))>.1,'effects did not resume alongside CD music'
   streams=json.loads(run('pactl','-f','json','list','sink-inputs').stdout)
   assert len([s for s in streams if s.get('properties',{}).get('application.process.id')==str(game.pid)])==1,'game opened competing audio outputs'
   tap('Shift_L+q');confirm_flight_quit(env,window);game.wait(timeout=10)
   assert game.returncode==0 and 'ERROR:' not in log.read_text() and 'unavailable' not in log.read_text(),log.read_text()
   print('Private live mixed audio passed: CD advances through configuration, mute/restart continues, effects mix and disable independently, one PCM device and clean shutdown')
 finally:
  for process in (game,capture,display):
   if process is not None and process.poll() is None:
    process.terminate()
    try:process.wait(timeout=3)
    except subprocess.TimeoutExpired:process.kill();process.wait()
  if module is not None:subprocess.run(['pactl','unload-module',module],check=True)
