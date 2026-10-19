#!/usr/bin/env python3
"""Check synthesised or CD music through the live window and a private audio sink."""
import argparse
import array
import json
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
p.add_argument('--rom-dir',type=Path)
p.add_argument('--cd-cue',type=Path)
p.add_argument('--cd-audio-dir',type=Path)
p.add_argument('--mission',help='check direct flight music instead of the opening presentation')
p.add_argument('--music',choices=('roland-lapc','soundblaster-fm','soundblaster-cd','roland-scc1a','roland-sc55'),default='roland-lapc')
a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='tfx-music-window-') as temporary:
 directory=Path(temporary);install=directory/'TFX';install.mkdir()
 for name in ('DID.DAT','TFX.DAT','START.EXE','TUNES'):(install/name).symlink_to((a.data_dir/name).resolve())
 (install/'TFX.CFG').write_bytes(('CONTROL=0\r\nMODEL=0\r\nSOUNDFX=0\r\nMUSIC='+('2' if a.music=='roland-lapc' else '1' if a.music in ('soundblaster-fm','soundblaster-cd') else '4')+'\r\nREALISM=0,0,1,1,0,0,0\r\nDETAIL=2\r\n').encode())
 sink='tfx_music_'+str(os.getpid());module=None;display=game=capture=None
 try:
  module=subprocess.check_output(['pactl','load-module','module-null-sink','sink_name='+sink,'rate=48000','channels=2'],text=True).strip()
  read,write=os.pipe();display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write)
  if not select.select([read],[],[],5)[0]:raise RuntimeError('private display did not start')
  number=os.read(read,64).decode().strip();os.close(read)
  env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_SESSION_TYPE='x11',PULSE_SINK=sink)
  for key in ('WAYLAND_DISPLAY','DBUS_SESSION_BUS_ADDRESS'):env.pop(key,None)
  def run(*args):return subprocess.run(args,env=env,check=True,capture_output=True,text=True,timeout=5)
  pcm=directory/'music.f32';log=directory/'game.log'
  with pcm.open('wb') as recording,log.open('w') as output:
   capture=subprocess.Popen(['parec','--device='+sink+'.monitor','--format=float32le','--rate=48000','--channels=2'],stdout=recording,stderr=subprocess.DEVNULL,env=env)
   game=subprocess.Popen([str((a.build/'tfx').resolve()),'--data-dir',str(install),'--music',a.music,'--scale','2',*(['--rom-dir',str(a.rom_dir.resolve())] if a.rom_dir else []),*(['--cd-cue',str(a.cd_cue.resolve())] if a.cd_cue else []),*(['--cd-audio-dir',str(a.cd_audio_dir.resolve())] if a.cd_audio_dir else []),*(['--mission',a.mission] if a.mission else [])],stdout=output,stderr=subprocess.STDOUT,env=env)
   # Route this process immediately after its initially silent device opens,
   # before the first presentation requests any music. Do not change defaults.
   routed=False;deadline=time.monotonic()+30
   while time.monotonic()<deadline and game.poll() is None:
    streams=json.loads(run('pactl','-f','json','list','sink-inputs').stdout)
    owned=[stream for stream in streams if stream.get('properties',{}).get('application.process.id')==str(game.pid)]
    if owned:
     assert len(owned)==1;run('pactl','move-sink-input',str(owned[0]['index']),sink);routed=True;break
    time.sleep(.01)
   assert routed,log.read_text()
   window=None;deadline=time.monotonic()+20
   while time.monotonic()<deadline and game.poll() is None:
    windows=subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True).stdout.splitlines()
    if windows:window=windows[0];break
    time.sleep(.05)
   assert window,log.read_text();run('xdotool','windowfocus','--sync',window)
   time.sleep(8)
   raw=pcm.read_bytes();raw=raw[:len(raw)//8*8];values=array.array('f');values.frombytes(raw)
   assert values and max(map(abs,values))>.001,('presentation music was silent',log.read_text())
   if a.music!='soundblaster-fm':assert any(abs(values[index]-values[index+1])>1e-5 for index in range(0,len(values),2)),'Stereo output was lost'
   else:assert all(values[index]==values[index+1] for index in range(0,len(values),2)),'FM dual mono changed'
   run('xdotool','keydown','Shift_L');run('xdotool','keydown','q');time.sleep(.2);run('xdotool','keyup','q');run('xdotool','keyup','Shift_L');confirm_flight_quit(env,window);game.wait(timeout=10)
   assert game.returncode==0 and 'ERROR:' not in log.read_text() and 'unavailable' not in log.read_text(),log.read_text()
   print('Private Pulse/X11 '+a.music+' music passed: '+('live flight bank' if a.mission else 'original presentation requests')+', live PCM and clean shutdown')
 finally:
  for process in (game,capture,display):
   if process is not None and process.poll() is None:
    process.terminate()
    try:process.wait(timeout=3)
    except subprocess.TimeoutExpired:process.kill();process.wait()
  if module is not None:subprocess.run(['pactl','unload-module',module],check=True)
