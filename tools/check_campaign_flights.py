#!/usr/bin/env python3
"""Reach campaign ejection/return through real private-window controls.

A disposable DOS pilot supplies the initial entry only. GDB reads actual flight
entry and FINISH results, never changes game state. Original parser handlers
predict the subsequent handoff from those observed results. This proves live
binding; independent complete-frame comparisons remain the flight oracle.
"""
import argparse
import json
import os
from pathlib import Path
import select
import struct
import subprocess
import sys
import tempfile
import time
from native_campaign import NativeCampaign
from window_input import move_game_pointer,confirm_flight_quit

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--research',type=Path,default=Path('../tfx-reverse'))
p.add_argument('--build',type=Path,default=Path('build'))
p.add_argument('--case',choices=('soapcol','todefa','todf17','todf22'))
p.add_argument('--report',type=Path)
a=p.parse_args()
program=next(x for x in json.loads((a.research/'analysis/presentations/programs.json').read_text()) if x['name']=='tfx')
named={f['name']:i for i,f in enumerate(program['functions'])}
reference=(a.research/'installs/cd_1994_09/TFX').resolve();reports=[]
sys.path.insert(0,str(a.research/'did-unpack'))
from did_unpack import Archive
archive=Archive.read(reference/'DID.DAT');entries={entry.identifier:entry for entry in archive.entries}
for case in ([a.case] if a.case else ('soapcol','todefa','todf17','todf22')):
 with tempfile.TemporaryDirectory(prefix='tfx-campaign-flight-') as temporary:
  root=Path(temporary);install=root/'TFX';install.mkdir()
  for name in ('DID.DAT','TFX.DAT'):(install/name).symlink_to(reference/name)
  (install/'TFX.CFG').write_bytes(b'CONTROL=0\r\nMODEL=0\r\nSOUNDFX=0\r\nMUSIC=0\r\nREALISM=0,0,1,1,0,0,0\r\nDETAIL=2\r\n')
  function=program['functions'][named[case]]
  call=next(n['index'] for n in function['nodes'] if n['type']==3 and bytes.fromhex(n['payload_hex'])[10:13]==bytes((1,named['mission'],0)))
  initial=dict(rank=1,lives=3,miss_val=1,sqd_val=3,soap_val=1)
  for node in function['nodes'][:call]:
   if node['type']==10:
    raw=bytes.fromhex(node['payload_hex']);ordinal=int.from_bytes(raw[:4],'little')
    if raw[4]==0:initial[program['variables'][ordinal]['name']]=int.from_bytes(raw[5:9],'little')
  pilot=bytearray()
  for v in program['variables']:pilot.extend(b'TEST'+bytes(17) if v['string_value'] else struct.pack('<I',initial.get(v['name'],0)))
  pilot.extend(bytes((30-len(program['variables']))*4));pilot.extend(struct.pack('<3H',0,named[case],call-1))
  (install/'TFXSAV.0').write_bytes(pilot)
  observer=root/'observe.gdb'
  observer.write_text('''set pagination off
set confirm off
set startup-with-shell off
set print thread-events off
python
import json
entry_mission='';entry_index=0;seen_flight=0;pilot_page=-1;flight_frames=0

def numbers(value):
 if value.type.code==gdb.TYPE_CODE_REF:value=value.referenced_value()
 children=list(gdb.default_visualizer(value).children())
 return {children[i][1]['_M_dataplus']['_M_p'].string():int(children[i+1][1]) for i in range(0,len(children),2)}

class Result(gdb.FinishBreakpoint):
 def __init__(self,value):
  super().__init__(internal=True)
  self.value=value
 def stop(self):
  print('RESULT '+json.dumps(numbers(self.value)),flush=True)
  return False

class Finish(gdb.Breakpoint):
 def stop(self):
  Result(gdb.parse_and_eval('campaign'))
  return False
Finish('tfx::missions::finish_program::execute')
end
break tfx::platform::fly_mission
commands
silent
python
entry_index+=1
entry_mission=gdb.parse_and_eval('options')['mission']['_M_dataplus']['_M_p'].string()
print('ENTRY '+json.dumps({'mission':entry_mission,'values':numbers(gdb.parse_and_eval('campaign'))}),flush=True)
end
continue
end
break tfx::flight::player_flight::advance
commands
silent
python
if seen_flight!=entry_index:
 seen_flight=entry_index
 flight_frames=0
flight_frames+=1
if flight_frames==3:print('FLIGHT '+str(entry_index),flush=True)
end
continue
end
break tfx::presentation::pilot_panel::advance
commands
silent
python
current=int(gdb.parse_and_eval('this->current'))
if not int(gdb.parse_and_eval('this->entering')) and current!=pilot_page:
 pilot_page=current
 print('PILOT_READY '+str(current),flush=True)
end
continue
end
break tfx::flight::ejection_sequence::advance_pilot if this->state.pilot.phase == tfx::flight::pilot_phase::parachute
commands
silent
printf "PARACHUTE\\n"
continue
end
run
''')
  read,write=os.pipe();display=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1024x768x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);os.close(write);game=None
  try:
   assert select.select([read],[],[],5)[0],'private display did not start'
   number=os.read(read,64).decode().strip();os.close(read)
   env=dict(os.environ,DISPLAY=':'+number,LIBGL_ALWAYS_SOFTWARE='1',XDG_SESSION_TYPE='x11',XMODIFIERS='@im=none')
   env['ASAN_OPTIONS']=env.get('ASAN_OPTIONS','')+':detect_leaks=0'
   for key in ('WAYLAND_DISPLAY','DBUS_SESSION_BUS_ADDRESS'):env.pop(key,None)
   def run(*command):return subprocess.run(command,env=env,capture_output=True,text=True,check=True,timeout=5)
   log=root/'game.log'
   def text():return log.read_text()
   def records(label):return [json.loads(line[len(label)+1:]) for line in text().splitlines() if line.startswith(label+' ')]
   with log.open('w') as output:
    game=subprocess.Popen(['gdb','--quiet','--batch','--return-child-result','-x',str(observer),'--args',str((a.build/'tfx').resolve()),'--data-dir',str(install),'--skip-intro','--control','keyboard','--scale','2'],cwd=root,env=env,stdout=output,stderr=subprocess.STDOUT)
    def wait(predicate,label,timeout=40):
     deadline=time.monotonic()+timeout
     while time.monotonic()<deadline and game.poll() is None:
      if predicate():return
      time.sleep(.05)
     raise AssertionError((case,label,text()[-6000:]))
    wait(lambda:bool(subprocess.run(['xdotool','search','--onlyvisible','--name','TFX (C|Redux)'],env=env,capture_output=True,text=True).stdout.strip()),'window')
    window=run('xdotool','search','--onlyvisible','--name','TFX (C|Redux)').stdout.splitlines()[0];run('xdotool','windowfocus','--sync',window)
    def tap(key):run('xdotool','keydown',key);time.sleep(.15);run('xdotool','keyup',key);time.sleep(.2)
    def title():return run('xdotool','getwindowname',window).stdout
    def click(x,y):
     move_game_pointer(env,window,x,y);run('xdotool','mousedown','1');time.sleep(.15);run('xdotool','mouseup','1');time.sleep(.2)
    wait(lambda:'PILOT_READY 0' in text(),'pilot menu');tap('Down');tap('Return');wait(lambda:'PILOT_READY 2' in text(),'pilot load list');tap('Return')
    script=archive.unpack(entries[archive.hash_name(f"missions/miss{initial['mission']}.mis")])
    default_weapons=b'DEFAULT_WEAPONS' in script.upper()
    if not default_weapons:
     wait(lambda:'Arming' in title(),'authored arming')
     time.sleep(1);click(101,198);click(197,198)
    wait(lambda:'FLIGHT 1' in text(),'flight after default arming')
    tap('Escape');time.sleep(.3)
    assert not records('RESULT'),(case,'first Escape ended the mission')
    tap('Escape')
    wait(lambda:'PARACHUTE' in text() or bool(records('RESULT')),'ejection/parachute',60)
    if not records('RESULT'):tap('Escape')
    wait(lambda:bool(records('RESULT')),'FINISH')
    result=records('RESULT')[0]
    print(case,'natural FINISH',result,flush=True)
    assert result['outcome'] in (1,3),(case,result)
    # Resume the real suspended mission call with the observed, naturally
    # produced FINISH values; only media/planner transport is supplied.
    native=NativeCampaign(reference,program);native.reset((named['mission'],3),result)
    caller=next((named[case],n['index']) for n in program['functions'][named[case]]['nodes'] if n['type']==3 and bytes.fromhex(n['payload_hex'])[10:13]==bytes((1,named['mission'],0)))
    native.image.write(0x1afdc,'<H',1);native.image.write(0x1ab68,'<I',native.addresses[caller])
    expected_save=None
    for _ in range(1000):
     boundary=native.run();assert boundary is not None,(case,'ended original parser')
     fi,ni,kind=boundary;node=program['functions'][fi]['nodes'][ni];resource=node.get('resource','')
     if kind==8 or kind==5 and resource in ('%mission','enrol'):break
     if kind==5 and resource=='spos':expected_save=native.save(2)
     native.complete()
    else:raise AssertionError((case,'native return did not yield'))
    expected=native.values()
    deadline=time.monotonic()+45
    while time.monotonic()<deadline and game.poll() is None:
     if resource=='%mission' and len(records('ENTRY'))>=2:break
     if resource=='enrol' and text().count('PILOT_READY 0')>=2:break
     if kind==8 and program['functions'][fi]['name'] in title():break
     tap('Escape')
    else:raise AssertionError((case,'campaign handoff not reached',boundary,resource,text()[-6000:]))
    if resource=='%mission':
     actual=records('ENTRY')[1]
     assert actual['values']=={key:struct.unpack('<i',struct.pack('<I',value))[0] for key,value in expected.items()},(case,actual,expected)
     assert actual['mission']=='miss'+str(expected['mission'])
     if 'Arming' in title():
      time.sleep(1);tap('Escape')
     wait(lambda:'FLIGHT 2' in text(),'retry flight')
    if expected_save is not None:assert (install/'TFXSAV.0').read_bytes()==expected_save,(case,'authored pilot save differs from original')
    tap('Shift_L+q')
    if resource=='%mission':confirm_flight_quit(env,window)
    game.wait(timeout=15)
    assert game.returncode==0 and 'Program received signal' not in text() and 'ERROR:' not in text(),text()[-5000:]
    reports.append(dict(case=case,mission=records('ENTRY')[0]['mission'],result=result,next_resource=resource,native_handoff=expected,authored_save_checked=expected_save is not None))
    print(f"Private campaign {case}: authored loadout/ejection/FINISH outcome {result['outcome']}, original {resource} handoff and retained pilot state passed",flush=True)
  finally:
   for process in (game,display):
    if process is not None and process.poll() is None:
     process.terminate()
     try:process.wait(timeout=3)
     except subprocess.TimeoutExpired:process.kill();process.wait()
if a.report:a.report.write_text(json.dumps(reports,indent=2)+'\n')
