"""Build the portable media catalogue after the engine captures finish."""
import csv, html, json, re
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
OUT=Path(__file__).resolve().parent
captions={
29:'Following the Eurofighter towards the amber lights of the Yugoslav city.',30:'Road lights and distant districts under the dark blue night sky.',31:'A level Eurofighter pass over the streets as the daylight fades.',32:'Green instruments and city lights beyond the Eurofighter canopy.',
33:'The Nighthawk heads towards Colombia’s illuminated streets.',34:'Looking across the city towards the mountains after dark.',35:'The Nighthawk’s silhouette against the fading light over Colombia.',36:'A night flight with live instruments in the Nighthawk’s virtual cockpit.',
37:'Following the Raptor over Libya, with a purple night sky ahead.',38:'Desert city lights beneath Libya’s red and purple horizon.',39:'The Raptor crosses the outskirts of the city at dusk.',40:'A view through the Raptor canopy over Libya at night.',
41:'Following the Eurofighter over the desert and coastline in the early light.',42:'A level Eurofighter profile against the pale morning landscape.',43:'Following the Nighthawk towards the coast at dusk.',44:'A low, level Nighthawk pass above the desert coast.',45:'The Raptor heads inland beneath the Libyan night sky.',46:'A Raptor profile with distant lights along the coast.',47:'The Raptor seen from behind over the pale desert.',48:'Morning light catches the Raptor above the coastal plain.',
49:'Gear down, following the Eurofighter towards the runway in the mountain valley.',50:'The Eurofighter’s undercarriage against the distant ridgeline.',51:'Following the Nighthawk towards the lit runway at dusk.',52:'A Nighthawk approach with the landing gear fully extended.',53:'The runway lights lead the Raptor into the valley after dark.',54:'Gear down beneath a dark blue sky, with the mountains behind.',55:'The illuminated runway is framed by the Eurofighter’s original 2D cockpit.',56:'Runway lights ahead through the live virtual cockpit.',57:'The valley runway ahead in the last of the daylight.',58:'A daylight approach to the mountain airfield, using the classic panel.',
59:'Following the Eurofighter above the native cloud layer.',60:'Looking across the Eurofighter’s live instruments towards the cloud tops.',61:'The Raptor seen from behind over an Atlantic cloud deck.',62:'The Raptor’s canopy and HUD against the clouds.',63:'A Nighthawk silhouette above the Colombian cloud deck.',64:'Cloud formations through the Nighthawk’s angular canopy.',65:'Following the Eurofighter over the clouds in the morning palette.',66:'The Eurofighter canopy frames the morning cloud tops.',67:'The Raptor above the lower storm cloud layer.',68:'Above the weather, looking across the Raptor’s live cockpit.',
69:'Looking to starboard at a Eurofighter flying alongside.',70:'Looking back over the Eurofighter’s shoulder towards its wingman.',71:'A Raptor alongside, seen through the canopy above the cloud deck.',72:'A trailing Raptor seen over the shoulder above the clouds.',73:'Looking to the right through the Nighthawk canopy towards the city lights.',74:'Looking to the left from the Nighthawk’s virtual cockpit at twilight.',75:'A Eurofighter wingman off the starboard side over the desert.',76:'Checking the trailing Eurofighter over the left shoulder.',
77:'A clear view of the Nighthawk’s open weapon bays.',78:'A Paveway separates beneath the Nighthawk over the Libyan coast.',79:'The Raptor’s internal bays exposed in an underside view.',80:'A Paveway below the Raptor, with the open bays still visible.',81:'Collapsed industrial buildings, flames and drifting smoke after a staged strike.',82:'Fire and dense smoke at the industrial complex as daylight fades.',83:'Following the Eurofighter past the burning depot.',84:'Following a descending Paveway towards the industrial buildings.',85:'A Tomahawk on its approach to the industrial complex.',86:'An AMRAAM leaving the Eurofighter, with the native smoke particles behind it.',87:'An AMRAAM release alongside the Raptor over the desert at dusk.',88:'An AMRAAM release alongside the Eurofighter over the Atlantic.',
89:'Following three MiG-29s over the Yugoslav landscape.',90:'Flying alongside a three-aircraft MiG-29 formation.',91:'A MiG-21 formation from behind, with the desert coast ahead.',92:'MiG-21s crossing the desert at dusk, seen from the side.',93:'Following an F-16 formation towards the city.',94:'F-16s flying alongside over the Colombian cityscape.',95:'Three Raptors seen from behind above the clouds.',96:'A Raptor formation alongside at the top of the cloud layer.',97:'The offshore platform’s native flare and smoke effects at twilight.',98:'A low view of the platform under the Atlantic storm sky.',99:'Carrier, frigate and battleship from close to the waterline.',100:'The carrier group ahead through the Eurofighter’s classic cockpit at twilight.',
101:'Runway edge lights stretch into Libya’s purple night.',102:'Nighthawk gear down against the Libyan night sky.',103:'A quiet desert airfield in the pale light of morning.',104:'The Raptor on short final in early morning light.',105:'Looking along the lit Colombian runway after dark.',106:'The Nighthawk on final, with mountains beneath the night sky.',107:'The mountain-valley airfield lights at twilight.',108:'The Eurofighter’s extended landing gear against the valley ridgeline.',
109:'A 65-degree Eurofighter bank over the city, afterburners lit.',110:'The Eurofighter on a wing in the mountain valley.',111:'A steep Raptor break beside the offshore platform.',112:'The Raptor nearly knife-edge above the cloud deck.',113:'A steep Nighthawk turn above the desert coast.',114:'A banked MiG-29 with the original manoeuvre-script wingtip trails and afterburners.',115:'The MiG-21’s native vortex sequence during a staged steep turn.',116:'An F-16 turn above Colombia, with wingtip trails and afterburner.',117:'Two MiG-29s breaking together, using their original vortex animation streams.',118:'The valley swings across the Eurofighter’s vector HUD in a steep bank.',119:'A sharply tilted Atlantic horizon behind the Raptor’s classic panel.',120:'Looking into a banked turn from the Raptor cockpit above the clouds.',121:'The Eurofighter pulls up above the valley with afterburners lit.',122:'The original pilot and deployed parachute above the city at twilight.',123:'Small impact scars and smoke beside the damaged industrial buildings.',124:'The native impact ring, sparks and fire beside a damaged building.'}
name={'efa':'Eurofighter','f22':'F-22 Raptor','f117':'F-117 Nighthawk'}
subjects={19:'MiG-29',20:'MiG-21',21:'Eurofighter, tanker',22:'F-22 Raptor',23:'F-117 Nighthawk',24:'Eurofighter, F-22 Raptor, F-117 Nighthawk',25:'Carrier group',26:'Oil platform',30:'City',34:'City',38:'City',81:'Industrial buildings',82:'Industrial buildings',84:'Paveway GBU-24',85:'AGM-109 Tomahawk',89:'MiG-29',90:'MiG-29',91:'MiG-21',92:'MiG-21',93:'F-16',94:'F-16',95:'F-22 Raptor',96:'F-22 Raptor',97:'Oil platform',98:'Oil platform',99:'Carrier group',101:'Airfield',103:'Airfield',105:'Airfield',107:'Airfield',114:'MiG-29',115:'MiG-21',116:'F-16',117:'MiG-29',122:'Pilot and parachute',123:'Industrial buildings',124:'Industrial buildings'}
# A hand-picked entry point; all images remain available.
picks={14,15,16,26,29,30,38,43,49,50,53,55,59,60,69,70,73,77,78,81,82,84,95,97,100,101,109,110,111,112,114,116,117,118,119,121,122,124}
manifest=json.loads((OUT/'manifest.json').read_text());scenes=manifest['scenes'];spec=json.loads((OUT/'scene-library.json').read_text())
for s in scenes:
 n=int(s['id'].split('-')[0]);r=s['recipe'];env=s['environment']
 if s['category']=='Formation':s['category']='Formations'
 if n in captions:s['caption']=captions[n]
 if n==74:s['title']='Nighthawk — looking to port'
 if n==73:s['title']='Nighthawk — looking to starboard'
 s['subjects']=subjects.get(n,name[s['aircraft']]);s['pick']=n in picks
 s['region']='Libya' if env.startswith('libya') else 'Colombia' if env.startswith('colum') else 'Atlantic' if env.startswith('sea') else 'Yugoslavia' if env.startswith('yugos') or env=='y_winter' else 'Somalia'
 s['light']='Night' if re.search(r'_n(?:c)?$',env) else 'Twilight' if env.endswith('_tw') or env=='seatwi' else 'Dawn' if env.endswith('_m') else 'Day'
 s['weather']='Snow' if env=='y_winter' else 'Storm' if env.endswith('_st') or env=='seastorm' else 'Clouds' if env.endswith(('_dc','_nc','_m')) or env=='seacloud' else 'Clear'
 view=re.search(r'^view (\w+)',r,re.M).group(1);s['view']='2D cockpit' if view=='cockpit' else 'Virtual cockpit' if view in ['virtual','left','right','rear'] else 'External'
 tags=[s['subjects'],s['region'],s['light'],s['weather'],s['view'],s['category']]
 for word in ['chase','gear','bays','missile','bomb','runway','city','vortex','vortices','shoulder','cloud','fire','smoke','bank','parachute']:
  if word in (s['id']+' '+s['caption']).lower():tags.append(word)
 if any('vortex' in t or 'vortices' in t for t in tags):tags.extend(['vortex','vortices'])
 s['tags']=list(dict.fromkeys(tags))
 s['staging']='In-engine staged composition; original models, scenery and animation. Not an unscripted combat recording.'
 # Keep local rerender settings paired with the revised descriptions.
 item=next(x for x in spec['scenes'] if x['id']==s['id'])
 for k in ['category','caption','title','subjects','pick','region','light','weather','view','tags','staging']:item[k]=s[k]
manifest['description']='A 4K TFX Redux screenshot gallery. Direct engine renders with staged cameras, formations and effects.'
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');(OUT/'scene-library.json').write_text(json.dumps(spec,indent=2)+'\n')
with (OUT/'captions.csv').open('w',newline='') as f:
 fields=['file','title','caption','subjects','region','light','weather','view','category','pick','tags','width','height','sha256','staging'];w=csv.DictWriter(f,fields);w.writeheader()
 for s in scenes:w.writerow({k:'; '.join(s[k]) if k=='tags' else s[k] for k in fields})
# Comfortable 24-frame contact sheets, in capture order.
folder=OUT/'contact-sheets';folder.mkdir(exist_ok=True);links=[]
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',16)
for start in range(0,len(scenes),24):
 batch=scenes[start:start+24];w,h=480,302;im=Image.new('RGB',(w*4,h*((len(batch)+3)//4)),(13,18,25));dr=ImageDraw.Draw(im)
 for i,s in enumerate(batch):
  x=(i%4)*w;y=(i//4)*h
  with Image.open(OUT/'thumbs'/f'{s["id"]}.jpg') as pic:im.paste(pic.resize((w,270),Image.Resampling.LANCZOS),(x,y))
  dr.text((x+8,y+278),s['id'],font=font,fill=(221,231,235))
 filename=f'{start+1:03}-{start+len(batch):03}.jpg';im.save(folder/filename,quality=94,subsampling=0);links.append(f'<a href="contact-sheets/{filename}">{start+1}–{start+len(batch)}</a>')
# The gallery can be browsed directly from file:// or a shared drive. No dependencies or remote requests.
e=html.escape
cards=[]
for i,s in enumerate(scenes):
 cards.append(f'''<article data-index="{i}"><button class="photo" data-index="{i}" aria-label="View {e(s['title'])}"><img src="thumbs/{s['id']}.jpg" alt="{e(s['caption'])}" width="640" height="360" loading="lazy"></button><div class="caption"><small>{e(s['subjects'])} · {e(s['region'])} · {s['light']}</small><h2>{e(s['title'])}</h2><p>{e(s['caption'])}</p><a href="{s['file']}">4K PNG ↗</a><a href="recipes/{s['id']}.scene">Recipe</a><button class="save" data-index="{i}" aria-pressed="false" aria-label="Shortlist {e(s['title'])}">☆</button></div></article>''')
select=lambda ident,label,values:f'<label>{label}<select id="{ident}"><option value="">All</option>'+''.join(f'<option>{e(v)}</option>' for v in values)+'</select></label>'
controls=select('category','Collection',dict.fromkeys(s['category'] for s in scenes))+select('view','Camera',['2D cockpit','Virtual cockpit','External'])+select('light','Light',['Day','Dawn','Twilight','Night'])
page=r'''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>TFX Redux · 4K media library</title><style>
:root{color-scheme:dark;font-family:system-ui,sans-serif;background:#0c1219;color:#e7eef1}*{box-sizing:border-box}body{margin:0}header,main,footer{max-width:1740px;margin:auto;padding:30px}header{padding-top:46px}.eyebrow,small{font-size:12px;color:#9bb7bc;letter-spacing:.04em}h1{font-size:clamp(36px,5vw,66px);margin:12px 0}header p{max-width:930px;color:#b3c2cc;line-height:1.6}a{color:#aee3d3;text-underline-offset:3px}header a{margin-right:12px}.toolbar{display:flex;gap:12px;align-items:end;flex-wrap:wrap;padding:24px 0 12px}label{font-size:12px;color:#a7bac6;display:grid;gap:6px}select,input,button{font:inherit;border:1px solid #3e5260;background:#182631;color:#e7eef1;border-radius:5px;padding:9px 12px}select{min-width:145px;max-width:100%}input{width:280px;max-width:100%}button{cursor:pointer}.toggle[aria-pressed=true]{background:#afe0ce;color:#0f2920}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:22px}article{background:#15212c;border:1px solid #293d4a;border-radius:7px;overflow:hidden}.photo{display:block;border:0;border-radius:0;padding:0;width:100%;background:black}.photo img{display:block;width:100%;height:auto}.caption{padding:16px;position:relative}h2{font-size:18px;margin:8px 0}article p{font-size:14px;line-height:1.5;color:#acbdc9;min-height:42px;margin:8px 0 14px}.caption a{font-size:12px;margin-right:20px}.save{float:right;padding:2px 8px;font-size:20px}.save[aria-pressed=true]{color:#f0cb75}#count{font-size:14px;color:#c9d6dd}main{padding-top:6px}[hidden]{display:none!important}dialog{width:98vw;max-width:98vw;max-height:98vh;padding:12px;background:#0b121a;color:#e7eef1;border:1px solid #3d5565}dialog::backdrop{background:#000e}dialog img{display:block;max-width:100%;height:auto;max-height:80vh;object-fit:contain;margin:auto}.bar{display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-top:12px}.bar span{flex:1;min-width:160px;font-size:14px}.fine{font-size:12px!important;color:#93aab7!important}footer{font-size:12px;color:#9aafb9;line-height:1.7}@media(max-width:600px){header,main,footer{padding:20px}.grid{grid-template-columns:1fr}.toolbar label{width:100%}input,select{width:100%}dialog img{max-height:70vh}}
</style><header><div class="eyebrow">LOST ENGINES · SCREENSHOT GALLERY</div><h1>TFX Redux</h1><p>COUNT frames · 3840 × 2160 · lossless PNG</p><p>Night approaches, desert mornings, cloud tops, hard turns and the quiet after a strike. Start with the highlights, or browse the whole collection.</p><p class="fine">Direct engine renders. Camera positions, formations and effects are staged; these are not unscripted combat recordings. No upscaling or retouching. Original 2D panel artwork retains its native detail.</p><div class="toolbar"><label>Search<input id="search" type="search" placeholder="City lights, vortices, Nighthawk…"></label>CONTROLS<button class="toggle" id="picks" aria-pressed="false">Highlights</button><button class="toggle" id="saved" aria-pressed="false">My shortlist</button><button id="reset">Reset filters</button></div><p id="count" role="status"></p></header><main><div class="grid">CARDS</div><p id="empty" hidden>No matches. Try another filter or search term.</p></main><footer><details><summary>Capture details and catalogue</summary><p><a href="https://github.com/Lost-Engines/tfx-redux/blob/master/docs/screenshot-gallery.md">Capture guide</a> · <a href="captions.csv">Captions &amp; catalogue CSV</a> · <a href="manifest.json">Capture metadata</a></p><p>Contact sheets: SHEETS</p></details><p>Click a photograph for the full frame. Arrow keys browse the current selection; Escape closes it. ☆ adds a frame to your shortlist in this browser.<br><a href="https://lostengines.com/">Lost Engines</a> · <a href="https://github.com/Lost-Engines/tfx-redux">TFX Redux on GitHub</a> · <a href="https://lost-engines.github.io/tfx-traced/">Inside TFX</a><br>This gallery also works offline. Original TFX artwork belongs to its respective rights holders.</p></footer><dialog aria-label="Screenshot viewer"><img id="full" alt=""><div class="bar"><button id="prev" aria-label="Previous image">←</button><button id="next" aria-label="Next image">→</button><span id="title"></span><a id="original">Open 4K PNG</a><button id="close">Close</button></div></dialog><script>
const shots=SHOTS;const $=s=>document.querySelector(s),dialog=$('dialog');let current=0,visible=shots.map((_,i)=>i),saved=new Set;try{saved=new Set(JSON.parse(localStorage.getItem('tfx-redux-shortlist')||'[]'))}catch{}
function syncStars(){document.querySelectorAll('.save').forEach(b=>{const on=saved.has(shots[+b.dataset.index].id);b.textContent=on?'★':'☆';b.setAttribute('aria-pressed',on)})}
function filter(){const words=$('#search').value.toLowerCase().trim().split(/\s+/).filter(Boolean);visible=[];shots.forEach((s,i)=>{const text=[s.title,s.caption,...s.tags].join(' ').toLowerCase();const on=['category','view','light'].every(k=>!$('#'+k).value||s[k]===$('#'+k).value)&&words.every(w=>text.includes(w))&&($('#picks').getAttribute('aria-pressed')!=='true'||s.pick)&&($('#saved').getAttribute('aria-pressed')!=='true'||saved.has(s.id));document.querySelector('article[data-index="'+i+'"]').hidden=!on;if(on)visible.push(i)});$('#count').textContent=visible.length+' of '+shots.length+' images';$('#empty').hidden=visible.length>0}
function show(i){current=i;const s=shots[i];$('#full').src=s.file;$('#full').alt=s.caption;$('#title').textContent=s.title+' · '+s.caption;$('#original').href=s.file;if(!dialog.open)dialog.showModal()}
function move(n){const p=visible.indexOf(current);if(visible.length)show(visible[(p+n+visible.length)%visible.length])}
document.querySelectorAll('.photo').forEach(b=>b.onclick=()=>show(+b.dataset.index));document.querySelectorAll('.save').forEach(b=>b.onclick=()=>{const id=shots[+b.dataset.index].id;saved.has(id)?saved.delete(id):saved.add(id);try{localStorage.setItem('tfx-redux-shortlist',JSON.stringify([...saved]))}catch{}syncStars();filter()});['category','view','light'].forEach(k=>$('#'+k).onchange=filter);$('#search').oninput=filter;['picks','saved'].forEach(k=>$('#'+k).onclick=()=>{$('#'+k).setAttribute('aria-pressed',$('#'+k).getAttribute('aria-pressed')!=='true');filter()});$('#reset').onclick=()=>{['category','view','light','search'].forEach(k=>$('#'+k).value='');['picks','saved'].forEach(k=>$('#'+k).setAttribute('aria-pressed','false'));filter()};$('#prev').onclick=()=>move(-1);$('#next').onclick=()=>move(1);$('#close').onclick=()=>dialog.close();document.addEventListener('keydown',e=>{if(!dialog.open)return;if(e.key==='ArrowRight'){e.preventDefault();move(1)}if(e.key==='ArrowLeft'){e.preventDefault();move(-1)}});syncStars();filter();
</script></html>'''
for k,v in [('COUNT',str(len(scenes))),('SHEETS',' · '.join(links)),('CONTROLS',controls),('CARDS',''.join(cards)),('SHOTS',json.dumps(scenes).replace('<',r'\u003c'))]:page=page.replace(k,v)
(OUT/'index.html').write_text(page)
(OUT/'README.txt').write_text(f'''TFX Redux — 4K screenshot gallery
{len(scenes)} original engine renders at 3840 x 2160; RGB lossless PNG.

Open index.html directly in a browser. Search, camera/light/collection filters,
highlights and a browser-local shortlist help select frames. Arrow keys browse
the selected images. Escape closes the full-frame viewer.

images/             Original 4K PNGs for social media and other uses.
thumbs/             Small JPEGs for browsing; not the source images.
contact-sheets/     Numbered overview sheets, up to 24 frames per sheet.
contact-sheet.jpg   One overview of the entire collection.
captions.csv        Captions, subjects, tags, resolution and image checksums.
manifest.json       Exact settings, recipes, source/build hashes and timestamps.
scene-library.json  Composition inventory for repeat captures (in the repository).
recipes/            Per-image staging recipes.

All views use Redux's ordinary rendering pipeline and the original game assets.
Cameras, formations, weapons, effects and poses are staged for composition.
No image upscaling, AI-generated imagery, retouching or compositing was used.
The original 2D panels retain their native pixel detail. Several scenes use
scripted animation states rather than recording unscripted gameplay.

The vortex shots use the MiG-29, MiG-21 and F-16's authored SSD events 15/16,
which are invoked by the original AI manoeuvre scripts. Those parameter meanings
do not transfer to all player aircraft. The scenes demonstrate the visual state,
not an independently measured G threshold.

Keep this directory together when copying to shared storage. It contains no
install media, game archives, ROMs or user saves. The selected images are tracked
in Git LFS and published at https://lost-engines.github.io/tfx-redux/.
Capture logs, scratch output and other capture directories remain local.
Original TFX artwork belongs to its respective rights holders.

To rerender selected scenes with the debug-enabled Redux build:
python3 tools/capture_showcase.py --data-dir /path/to/TFX \\
  --manifest /path/to/gallery/scene-library.json --output /path/to/gallery \\
  --only SCENE_ID

The builder always uses private temporary preferences. Re-rendering regenerates
the basic catalogue; the supplied packaging script can restore this expanded
catalogue. It requires Python 3 and Pillow, only for rebuilding, not browsing.
''')
print(f'Packaged {len(scenes)} full-resolution images; {len(links)} contact sheets; {len(picks)} highlights')
