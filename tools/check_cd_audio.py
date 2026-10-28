#!/usr/bin/env python3
"""Check CUE boundaries and streamed CD/rip PCM against independent source bytes and FFmpeg."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import wave
import numpy as np

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,default=Path('build'))
p.add_argument('--reference',type=Path,help='Optional tfx-reverse tree for original ISO/installer comparisons')
a=p.parse_args();probe=(a.build/'cd_probe').resolve()
def run(*args,ok=True):
 result=subprocess.run([str(probe),*map(str,args)],capture_output=True,text=True,timeout=45)
 assert (result.returncode==0)==ok,(args,result.stdout,result.stderr)
 return result.stdout
with tempfile.TemporaryDirectory(prefix='tfx-cd-check-') as temporary:
 root=Path(temporary);rate=44100;sector=2352;frames_per_sector=588
 def samples(sectors,frequency):
  x=np.arange(sectors*frames_per_sector)/rate
  return np.column_stack((np.sin(2*np.pi*frequency*x)*16384,np.cos(2*np.pi*frequency*x)*8192)).astype('<i2')
 first=samples(6,440);second=samples(4,660)
 gap=bytes([0xff,0x7f])*sector
 payload=bytes(sector*20)+gap[:sector]+first.tobytes()+gap+second.tobytes()
 (root/'shared image.bin').write_bytes(payload)
 single=root/'single.cue';single.write_text('''FILE "shared image.bin" BINARY
  TRACK 01 MODE1/2352
    INDEX 01 00:00:00
  TRACK 02 AUDIO
    INDEX 00 00:00:20
    INDEX 01 00:00:21
  TRACK 03 AUDIO
    INDEX 00 00:00:27
    INDEX 01 00:00:29
''')
 expected=['1 0 0 20 0','2 1 21 6 21','3 1 29 4 29']
 assert run('catalog',single).splitlines()==expected
 run('render',single,1,rate,10,root/'not-audio.f32',ok=False)
 def render(source,track,sample_rate,frames):
  output=root/'render.f32';status=run('render',source,track,sample_rate,frames,output)
  values=np.frombuffer(output.read_bytes(),dtype='<f4').reshape(-1,2)
  assert values.shape==(frames,2)
  return values,status
 for number,pcm in ((2,first),(3,second)):
  actual,status=render(single,number,rate,len(pcm)+64)
  assert np.array_equal(actual[:len(pcm)],pcm.astype(np.float32)/32768),(number,float(np.max(np.abs(actual[:len(pcm)]-pcm/32768))))
  assert not np.any(actual[len(pcm):]) and status.strip()=='playing 0'
 print('CUE: shared BIN, INDEX 00/01 pregaps, physical track selection, complete bit-exact stereo PCM and exact EOF passed')
 (root/'data.bin').write_bytes(bytes(sector*20))
 (root/'audio2.bin').write_bytes(gap[:sector]+first.tobytes())
 (root/'audio3.bin').write_bytes(gap+second.tobytes())
 split=root/'split.cue';split.write_text('''FILE "data.bin" BINARY
  TRACK 01 MODE1/2352
    INDEX 01 00:00:00
FILE "audio2.bin" BINARY
  TRACK 02 AUDIO
    INDEX 00 00:00:00
    INDEX 01 00:00:01
FILE "audio3.bin" BINARY
  TRACK 03 AUDIO
    INDEX 00 00:00:00
    INDEX 01 00:00:02
''')
 assert run('catalog',split).splitlines()==['1 0 0 20 0','2 1 1 6 21','3 1 2 4 29']
 actual,status=render(split,2,rate,len(first)+64)
 assert np.array_equal(actual[:len(first)],first.astype(np.float32)/32768)
 explicit=root/'gap.cue';explicit.write_text(split.read_text().replace('  TRACK 02 AUDIO','  TRACK 02 AUDIO\n    PREGAP 00:01:00'))
 assert run('catalog',explicit).splitlines()[1:] == ['2 1 1 6 96','3 1 2 4 104']
 invalid=root/'bad.cue';invalid.write_text('FILE "audio2.bin" BINARY\n  TRACK 02 AUDIO\n')
 run('catalog',invalid,ok=False)
 invalid.write_text('FILE "audio2.bin" BINARY\n  TRACK 02 AUDIO\n    INDEX 01 00:99:00\n')
 run('catalog',invalid,ok=False)
 (root/'partial.bin').write_bytes(b'partial sector');invalid.write_text('FILE "partial.bin" BINARY\n  TRACK 02 AUDIO\n    INDEX 01 00:00:00\n')
 run('catalog',invalid,ok=False)
 run('read',single,'DID.DAT',root/'bad-iso',ok=False)
 print('CUE: split BINs, explicit pregap timing, missing indices, invalid times, partial sectors and invalid ISO rejection passed')
 wav=root/'source.wav'
 with wave.open(str(wav),'wb') as output:
  output.setparams((2,2,rate,0,'NONE','not compressed'));output.writeframes(first.tobytes())
 reference_48,status=render(single,2,48000,4000)
 for extension,codec in [('wav','pcm_s16le'),('flac','flac'),('ogg','libvorbis'),('mp3','libmp3lame')]:
  directory=root/extension;directory.mkdir();file=directory/('TrAcK02.'+extension.upper())
  subprocess.run(['ffmpeg','-nostdin','-v','error','-i',str(wav),'-c:a',codec,str(file)],check=True)
  expected_bytes=subprocess.check_output(['ffmpeg','-nostdin','-v','error','-i',str(file),'-f','f32le','-ac','2','-ar',str(rate),'-'])
  reference=np.frombuffer(expected_bytes,dtype='<f4').reshape(-1,2)
  actual,status=render(directory,2,rate,max(len(reference),len(first))+8192)
  # Different compliant Vorbis/MP3 decoders can round differently, but their
  # samples must retain timing, channel order and decoded amplitude.
  tolerance=0 if extension in ('wav','flac') else 2e-5
  error=float(np.max(np.abs(actual[:len(reference)]-reference)))
  assert error<=tolerance,(extension,error,len(reference),len(first))
  assert not np.any(actual[max(len(reference),len(first))+2048:]),extension
  assert status.strip()=='playing 0'
  if extension in ('wav','flac'):
   converted,status=render(directory,2,48000,4000)
   assert np.array_equal(converted,reference_48),(extension,'image/rip resampling differs')
  print(f'Rip {extension}: numbered/case-insensitive discovery, independent FFmpeg PCM, EOF and stereo passed (maximum error {error:g})')
 if a.reference:
  research=a.reference.resolve();cue=research/'install media/cd_1994_09/tfx.cue'
  inv=json.loads((research/'analysis/version-inventory.json').read_text())['images']['alt_versions/tfx_cd_alt1/TFX.cue']['files']
  for name,metadata in inv.items():
   output=root/'iso-file';run('read',cue,name,output)
   assert output.stat().st_size==metadata['size'] and hashlib.sha256(output.read_bytes()).hexdigest()==metadata['sha256'],name
  destination=root/'installed';destination.mkdir();(destination/'TFX.CFG').write_bytes(b'preserve user settings');(destination/'TFXSAV.0').write_bytes(b'preserve pilot')
  run('install',cue,destination)
  for name in ('DID.DAT','TFX.DAT','START.EXE','GUS.COM','TFX.ADV','TUNES/INTRO.ADL','TUNES/INTRO.RLD','TUNES/INTRO.SCC'):
   assert (destination/name).read_bytes()==(research/'installs/cd_1994_09/TFX'/name).read_bytes(),name
  assert (destination/'TFX.CFG').read_bytes()==b'preserve user settings' and (destination/'TFXSAV.0').read_bytes()==b'preserve pilot'
  # Reset only this private fixture to check the new-install path too.
  (destination/'TFX.CFG').unlink()
  run('install',cue,destination)
  config=dict(line.split('=',1) for line in (destination/'TFX.CFG').read_text().splitlines())
  assert config['CONTROL']=='0' and config['MODEL']=='3' and config['REALISM']=='2,1,1,1,1,1,1', config
  assert (destination/'TFXSAV.0').read_bytes()==b'preserve pilot'
  print('Reference: all 16 ISO files and eight runtime assets match; settings/pilots preserved; fresh Redux realism defaults passed')
