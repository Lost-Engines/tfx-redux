#!/usr/bin/env python3
"""Check game/ROM fetching against a private HTTP fixture, without game data or public downloads."""
import argparse
import collections
import hashlib
import http.server
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading
import zipfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--script',type=Path,default=Path(__file__).resolve().parents[1]/'scripts'/('fetch-assets.bat' if os.name=='nt' else 'fetch-assets.sh'))
args=parser.parse_args()
with tempfile.TemporaryDirectory(prefix='tfx-fetch-test-') as temporary:
    root=Path(temporary);scripts=root/'scripts';scripts.mkdir()
    script=scripts/args.script.name;shutil.copy2(args.script,script)
    payloads={'cd/track01.bin':b'reference data track','cd/track02.bin':b'reference audio track','cd/tfx.cue':b'FILE \"track01.bin\" BINARY\r\n','keep.rom':b'valid existing ROM','repair.rom':b'replacement bytes','new.rom':b'new ROM','first.bin':b'first ZIP ROM','second.bin':b'second ZIP ROM','qsound/MAIN.PVL':b'bounded Creative firmware archive'}
    bundle=root/'bundle.zip'
    with zipfile.ZipFile(bundle,'w',compression=zipfile.ZIP_DEFLATED) as archive:
        for name in ('first.bin','second.bin'):archive.writestr('roms/'+name,payloads[name])
        archive.writestr('MAIN.PVL',payloads['qsound/MAIN.PVL'])
        archive.writestr('unrelated/leave-me-out.bin',b'not needed')
    requests=collections.Counter();agents=[];cue_errors=[];fail_archive=False;corrupt=False
    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self,*unused):pass
        def do_GET(self):
            cue=active_destination/'cd/tfx.cue'
            if not cue.is_file() or cue.read_bytes()!=payloads['cd/tfx.cue']:
                cue_errors.append(self.path)
            requests[self.path]+=1;agents.append(self.headers.get('User-Agent'))
            if (self.path=='/repair.rom' and requests[self.path]==1) or (self.path=='/bundle.zip' and fail_archive):
                self.send_error(503);return
            body=bundle.read_bytes() if self.path=='/bundle.zip' else b'wrong bytes' if self.path=='/repair.rom' and corrupt else payloads[self.path[1:]]
            self.send_response(200);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:
        url=f'http://127.0.0.1:{server.server_port}'
        manifest=[];game_manifest=[]
        for name,data in payloads.items():
            if name=='cd/tfx.cue':continue
            if name.startswith('cd/'):
                game_manifest.append(f'{hashlib.sha256(data).hexdigest()} {name} {url}/{name} -')
                continue
            archive=name.endswith('.bin') or name=='qsound/MAIN.PVL'
            member='MAIN.PVL' if name=='qsound/MAIN.PVL' else 'roms/'+name
            manifest.append(f'{hashlib.sha256(data).hexdigest()} {name} {url}/'+('bundle.zip '+member if archive else name+' -'))
        # Windows packages have CRLF manifests; Bash must also accept those.
        (scripts/'game-files.txt').write_bytes(('\r\n'.join(game_manifest)+'\r\n').encode())
        (scripts/'cd-cue.txt').write_bytes(payloads['cd/tfx.cue'])
        (scripts/'roms.txt').write_bytes(('\r\n'.join(['# test ROMs',*manifest])+'\r\n').encode())
        destination=root/'destination with spaces !';destination.mkdir()
        active_destination=destination
        (destination/'keep.rom').write_bytes(payloads['keep.rom']);os.utime(destination/'keep.rom',ns=(1000000000,1000000000))
        original=(destination/'keep.rom').stat()
        (destination/'repair.rom').write_bytes(b'damaged ROM')
        def invoke(target=None,env=None):
            if os.name=='nt':
                command=f'"{os.environ.get("COMSPEC","cmd.exe")}" /d /s /c ""{script}"'
                if target is not None:command+=f' "{target}"'
                command+='"'
            else:
                command=['/bin/bash',str(script),*([str(target)] if target is not None else [])]
            return subprocess.run(command,cwd=root,env=env,capture_output=True,text=True,timeout=120)
        def run(expected=0,env=None):
            result=invoke(destination,env)
            assert result.returncode==expected,(result.returncode,result.stdout,result.stderr)
            assert not cue_errors,('network request before cue copy',cue_errors)
            assert not list(destination.glob('.fetch-assets*')),'temporary download files were left behind'
            return result
        def verify():
            for name,data in payloads.items():assert (destination/name).read_bytes()==data,name
            assert {path.relative_to(destination).as_posix() for path in destination.rglob('*') if path.is_file()}==set(payloads),'unrelated archive files were extracted'
        run();verify()
        assert (destination/'keep.rom').stat().st_mtime_ns==original.st_mtime_ns,'valid existing ROM was replaced'
        assert requests=={'/cd/track01.bin':1,'/cd/track02.bin':1,'/repair.rom':2,'/new.rom':1,'/bundle.zip':1},requests
        assert set(agents)=={'Lost Engines TFX Redux'},agents
        before=requests.copy();run();assert requests==before,'verified ROMs were downloaded again'
        print('Fetch: missing/damaged ROM repair, HTTP retry, user agent, one shared archive, selective extraction, CRLF manifest, spaced paths and cached reuse passed')
        for ending in (b'\n',b'\r\n'):
            payloads['cd/tfx.cue']=b'REM bundled text needs no checksum'+ending+b'FILE "track01.bin" BINARY'+ending
            (scripts/'cd-cue.txt').write_bytes(payloads['cd/tfx.cue'])
            (destination/'cd/tfx.cue').write_bytes(b'old cue')
            before=requests.copy();run();verify()
            assert requests==before,'cue copy caused downloads'
        (scripts/'cd-cue.txt').unlink()
        before=requests.copy();run(1)
        assert requests==before,'missing bundled cue did not stop fetching'
        (scripts/'cd-cue.txt').write_bytes(payloads['cd/tfx.cue'])
        (destination/'cd/tfx.cue').unlink();(destination/'cd/tfx.cue').mkdir()
        run(1)
        assert requests==before,'blocked cue destination did not stop fetching'
        assert not list((destination/'cd/tfx.cue').iterdir()),'cue was copied into a directory instead'
        (destination/'cd/tfx.cue').rmdir();run();verify()
        print('Fetch: bundled LF/CRLF cue copied before downloads, refreshed independently, and copy failures stop fetching')
        corrupt=True;(destination/'repair.rom').write_bytes(b'old damaged copy')
        run(1);assert (destination/'repair.rom').read_bytes()==b'old damaged copy','failed verification replaced the original'
        corrupt=False;run();verify()
        print('Fetch: incorrect downloads fail verification, preserve existing files, clean staging and recover on rerun')
        for name in ('first.bin','second.bin'):(destination/name).unlink()
        fail_archive=True;before=requests['/bundle.zip'];run(1)
        assert requests['/bundle.zip']-before==4,'a failed shared archive was retried separately for each ROM'
        fail_archive=False;run();verify()
        print('Fetch: shared archive failures produce a nonzero exit and one retry sequence; rerun restores the missing ROMs')
        # The no-argument helper must agree with the runtime, independently of
        # its own location and the directory from which it was launched.
        for xdg in (str(root/'user data with spaces'),'relative/data','') if os.name!='nt' else ():
            home=root/'home with spaces'
            default=Path(xdg)/'tfx'if Path(xdg).is_absolute()else home/'.local/share/tfx'
            active_destination=default
            env=dict(os.environ,HOME=str(home),XDG_DATA_HOME=xdg)
            result=invoke(env=env)
            assert result.returncode==0,(xdg,result.stdout,result.stderr)
            for name,data in payloads.items():assert (default/name).read_bytes()==data,(xdg,name)
            assert not (root/'roms').exists(),'helper still defaults to project-local ROMs'
            assert not list(default.glob('.fetch-assets*'))
            assert 'Use --rom-dir'not in result.stdout
        if os.name=='nt':
            active_destination=root
            result=invoke()
            assert result.returncode==0,(result.stdout,result.stderr)
            for name,data in payloads.items():assert (root/name).read_bytes()==data,name
            assert not list(root.glob('.fetch-assets*'))
            print('Fetch: Windows default destination is the package directory')
        else:
            print('Fetch: no-argument XDG/HOME destinations, relative XDG rejection and launch-directory independence passed')
        active_destination=destination
        if os.name!='nt' and shutil.which('shasum'):
            commands=root/'mac-style-path';commands.mkdir()
            for command in ('curl','unzip','dirname','mkdir','mktemp','rm','mv','cp','shasum'):(commands/command).symlink_to(shutil.which(command))
            env=dict(os.environ,PATH=str(commands));(destination/'new.rom').unlink()
            run(env=env);verify()
            print('Fetch: macOS shasum fallback passed with sha256sum absent from PATH')
        assert not cue_errors,('network request before cue copy',cue_errors)
    finally:
        server.shutdown();server.server_close();thread.join(timeout=5)
