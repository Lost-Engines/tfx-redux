#!/usr/bin/env python3
"""Render a reproducible local Redux screenshot gallery. Requires Pillow and a debug-enabled build."""
import argparse
from datetime import datetime, timezone
import hashlib
import html
import json
import re
from pathlib import Path
import subprocess
import tempfile
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT/'build')
    parser.add_argument('--data-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT/'captures/showcase-4k')
    parser.add_argument('--manifest', type=Path, default=ROOT/'tools/showcase/scenes.json')
    parser.add_argument('--only', nargs='+', help='Scene IDs to render; existing entries are retained.')
    parser.add_argument('--preview', action='store_true', help='960x540 previews in output/previews; never replace 4K images.')
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text())
    scenes = manifest['scenes']
    ids = [s['id'] for s in scenes]
    if len(ids)!=len(set(ids)) or any(not re.fullmatch(r'[a-z0-9][a-z0-9_-]*',sid) for sid in ids):
        parser.error('Scene IDs must be unique lowercase filenames without directory separators.')
    if args.only:
        unknown = set(args.only) - {s['id'] for s in scenes}
        if unknown: parser.error('Unknown scene IDs: '+', '.join(sorted(unknown)))
        scenes = [s for s in scenes if s['id'] in args.only]
    binary = (args.build/'tfx').resolve()
    data = args.data_dir.resolve()
    for name in ('TFX.DAT','DID.DAT'):
        if not (data/name).is_file(): parser.error(f'Missing {data/name}')
    out = args.output.resolve()
    if args.preview: out /= 'previews'
    for folder in ('images','thumbs','recipes','logs'): (out/folder).mkdir(parents=True,exist_ok=True)
    size = (960,540) if args.preview else (3840,2160)
    entries = {}
    if (out/'manifest.json').exists():
        entries = {s['id']:s for s in json.loads((out/'manifest.json').read_text())['scenes']}
    binary_hash = hashlib.sha256(binary.read_bytes()).hexdigest()
    asset_hashes = {name:hashlib.sha256((data/name).read_bytes()).hexdigest() for name in ('TFX.DAT','DID.DAT')}
    revision = subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    dirty = bool(subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],cwd=ROOT,text=True))
    # Never run the renderer against the user's writable preferences or saves.
    with tempfile.TemporaryDirectory(prefix='tfx-showcase-') as temp:
        private = Path(temp)
        for name in ('DID.DAT','TFX.DAT'): (private/name).symlink_to(data/name)
        (private/'TFX.CFG').write_text('SOUNDFX=0\nMUSIC=0\nCONTROL=0\nMODEL=3\nDETAIL=2\nREALISM=2,1,1,1,1,1,1\nHUDCOL=125\n')
        for i, scene in enumerate(scenes,1):
            sid = scene['id']
            recipe = out/'recipes'/f'{sid}.scene'
            recipe.write_text('# '+scene['title']+'\n# Photographic staging, not an unscripted gameplay recording.\n'+scene['recipe'])
            ppm = private/'frame.ppm'
            options = ['--mission',str(scene['mission']),'--environment',scene['environment'],'--music','none','--control','keyboard','--flight-model','military-spec','--resolution',f'{size[0]}x{size[1]}','--fov',str(scene['fov']),'--draw-distance',str(scene['draw_distance']),'--lod-scale',str(scene['lod_scale'])]
            command = [str(binary),'--data-dir',str(private),*options,'--capture-scene',str(recipe),'--screenshot',str(ppm)]
            print(f'[{i}/{len(scenes)}] {sid} ({size[0]}×{size[1]})',flush=True)
            with (out/'logs'/f'{sid}.txt').open('w') as log:
                subprocess.run(command,cwd=private,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=300)
            with Image.open(ppm) as image:
                assert image.size == size, (sid,image.size)
                colours=image.getcolors(maxcolors=8)
                if colours is not None and len(colours)<8: raise RuntimeError(f'{sid}: empty/blank frame')
                image.convert('RGB').save(out/'images'/f'{sid}.png',compress_level=9)
                thumb=image.copy();thumb.thumbnail((640,360),Image.Resampling.LANCZOS)
                thumb.convert('RGB').save(out/'thumbs'/f'{sid}.jpg',quality=90,subsampling=0)
            png=out/'images'/f'{sid}.png'
            entries[sid]={**scene,'width':size[0],'height':size[1],'file':f'images/{sid}.png','sha256':hashlib.sha256(png.read_bytes()).hexdigest(),'bytes':png.stat().st_size,'revision':revision,'working_tree_changes':dirty,'renderer_sha256':binary_hash,'source_sha256':asset_hashes,'options':options,'captured_utc':datetime.now(timezone.utc).isoformat()}
            write_gallery(out,manifest,entries,args.preview)
    print(out/'index.html')

def write_gallery(out,manifest,entries,preview):
    ordered=[entries[s['id']] for s in manifest['scenes'] if s['id'] in entries]
    (out/'manifest.json').write_text(json.dumps({'description':manifest['description'],'preview':preview,'scenes':ordered},indent=2)+'\n')
    cards=[]
    for s in ordered:
        e=html.escape
        cards.append(f'''<article data-category="{e(s['category'])}"><button class="photo" data-index="{len(cards)}" aria-label="View {e(s['title'])}"><img src="thumbs/{s['id']}.jpg" alt="{e(s['caption'])}" width="640" height="360" loading="lazy"></button><div class="caption"><small>{e(s['category'])} · {e(s['environment'])}</small><h2>{e(s['title'])}</h2><p>{e(s['caption'])}</p><a href="{s['file']}">Open {s['width']} × {s['height']} PNG</a> <a href="recipes/{s['id']}.scene">Scene recipe</a></div></article>''')
    categories=['All',*dict.fromkeys(s['category'] for s in ordered)]
    buttons=''.join(f'<button class="filter" aria-pressed="{str(i==0).lower()}" data-category="{html.escape(c)}">{html.escape(c)}</button>' for i,c in enumerate(categories))
    (out/'index.html').write_text('''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>TFX Redux · Screenshot library</title><style>
:root{color-scheme:dark;font-family:system-ui,sans-serif;background:#0d1219;color:#e6eaf0}*{box-sizing:border-box}body{margin:0}header,main{max-width:1740px;margin:auto;padding:32px}header{padding-top:54px}small,.eyebrow{color:#9eb5b7;font-size:12px;letter-spacing:.08em}h1{font-size:clamp(32px,5vw,62px);margin:10px 0}header p{max-width:900px;line-height:1.6;color:#b3c1ce}a{color:#afded1;text-underline-offset:3px}nav{display:flex;gap:9px;flex-wrap:wrap;margin-top:25px}button{font:inherit;cursor:pointer;color:inherit;background:#1c2933;border:1px solid #40515d;border-radius:5px;padding:9px 15px}.filter[aria-pressed=true]{background:#b9e5cf;color:#11251c;border-color:#b9e5cf}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:24px}article{border:1px solid #293744;border-radius:8px;overflow:hidden;background:#141e28}.photo{display:block;border:0;border-radius:0;padding:0;width:100%;background:#000}.photo img{width:100%;height:auto;display:block}.caption{padding:18px}h2{font-size:20px;margin:9px 0}article p{font-size:14px;line-height:1.5;color:#abb9c7;min-height:42px}.caption a{font-size:12px;margin-right:15px}[hidden]{display:none!important}dialog{max-width:98vw;max-height:98vh;width:98vw;border:1px solid #354858;background:#0b1118;padding:12px}dialog::backdrop{background:#000d}dialog img{display:block;max-width:100%;height:auto;max-height:82vh;margin:auto;object-fit:contain}.bar{display:flex;align-items:center;gap:12px;padding-top:12px;flex-wrap:wrap}.bar span{flex:1}.note{font-size:13px}footer{padding:24px 32px;color:#97a8b9;font-size:13px}@media(max-width:600px){header,main{padding:20px}.grid{grid-template-columns:1fr}}
</style><header><div class="eyebrow">LOST ENGINES · LOCAL RESOURCE LIBRARY</div><h1>TFX Redux</h1><p>'''+('Composition previews · 960 × 540' if preview else '28 scenes in 4K · 3840 × 2160 · Lossless PNG').replace('28 scenes',f'{len(ordered)} scenes')+'''</p><p>Three aircraft, classic and virtual cockpits, mountain flights, city lights and offshore scenery. These are direct engine renders: camera poses and formations are staged for composition, with the original game’s models, terrain, palettes and animation routines.</p><p class="note">Full frames are rendered at the stated resolution, without upscaling, retouching or added graphics. The 2D cockpit artwork retains its original pixel resolution. This is a local gallery; no game archives are included.</p><p><a href="contact-sheet.jpg">Contact sheet</a> · <a href="manifest.json">Capture settings and checksums</a></p><nav aria-label="Filter screenshots">'''+buttons+'''</nav></header><main><div class="grid">'''+''.join(cards)+'''</div></main><footer>Click an image to inspect it. Arrow keys browse; Escape closes. Full-resolution PNG links open the original captures.</footer><dialog aria-label="Screenshot viewer"><img id="full" alt=""><div class="bar"><button id="prev" aria-label="Previous image">←</button><button id="next" aria-label="Next image">→</button><span id="title"></span><a id="original">Open full PNG</a><button id="close">Close</button></div></dialog><script>
const shots='''+json.dumps(ordered).replace('<',r'\u003c')+''';let current=0;const dialog=document.querySelector('dialog');function show(i){current=(i+shots.length)%shots.length;const s=shots[current];document.querySelector('#full').src=s.file;document.querySelector('#full').alt=s.caption;document.querySelector('#title').textContent=s.title+' · '+s.width+' × '+s.height;document.querySelector('#original').href=s.file;if(!dialog.open)dialog.showModal()}document.querySelectorAll('.photo').forEach(b=>b.onclick=()=>show(+b.dataset.index));document.querySelector('#close').onclick=()=>dialog.close();document.querySelector('#prev').onclick=()=>show(current-1);document.querySelector('#next').onclick=()=>show(current+1);document.addEventListener('keydown',e=>{if(!dialog.open)return;if(e.key==='ArrowRight'){e.preventDefault();show(current+1)}if(e.key==='ArrowLeft'){e.preventDefault();show(current-1)}});document.querySelectorAll('.filter').forEach(b=>b.onclick=()=>{document.querySelectorAll('.filter').forEach(x=>x.setAttribute('aria-pressed',x===b));document.querySelectorAll('article').forEach(a=>a.hidden=b.dataset.category!=='All'&&a.dataset.category!==b.dataset.category)});
</script></html>''')
    w,h=384,242
    sheet=Image.new('RGB',(w*4,h*((len(ordered)+3)//4)),(13,18,25));draw=ImageDraw.Draw(sheet)
    for i,s in enumerate(ordered):
        with Image.open(out/'thumbs'/f"{s['id']}.jpg") as image:
            image=image.resize((w,216),Image.Resampling.LANCZOS)
            sheet.paste(image,((i%4)*w,(i//4)*h))
        draw.text(((i%4)*w+8,(i//4)*h+221),s['id'],fill=(225,235,244))
    sheet.save(out/'contact-sheet.jpg',quality=92,subsampling=0)

if __name__=='__main__':main()
