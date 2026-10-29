#!/usr/bin/env python3
"""Validate and stage only the curated screenshot gallery for GitHub Pages."""
import argparse
import csv
import hashlib
import io
import json
from html.parser import HTMLParser
from pathlib import Path, PurePosixPath
import re
import struct
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
GALLERY = ROOT / 'captures/showcase-4k'


def read_file(source, relative):
    path = source / relative
    for part in (path, *path.parents):
        if part.is_symlink():
            raise ValueError(f'Symlinks are not published: {path}')
    if not path.is_file():
        raise ValueError(f'Missing gallery file: {relative}')
    data = path.read_bytes()
    if data.startswith(b'version https://git-lfs.github.com/spec/v1'):
        raise ValueError(f'Unresolved Git LFS pointer: {relative}; run git lfs pull')
    return data


class Links(HTMLParser):
    def __init__(self):
        super().__init__()
        self.paths = set()

    def handle_starttag(self, tag, attrs):
        for key, value in attrs:
            if key not in ('href', 'src') or not value:
                continue
            url = urlsplit(value)
            if url.scheme or url.netloc:
                continue
            path = unquote(url.path)
            if not path:
                continue
            if path.startswith('/') or '..' in PurePosixPath(path).parts:
                raise ValueError(f'Link escapes the published gallery: {value}')
            self.paths.add(path)


def collect(source):
    # No directory recursion: each published file has a named purpose here.
    names = {'index.html', 'README.txt', 'manifest.json', 'captions.csv', 'contact-sheet.jpg'}
    manifest = json.loads(read_file(source, 'manifest.json'))
    scenes = manifest['scenes']
    if manifest.get('preview') or not scenes:
        raise ValueError('Only a non-empty, full-resolution gallery can be published')
    ids = set()
    for scene in scenes:
        ident = scene['id']
        if not re.fullmatch(r'[0-9]+-[a-z0-9_]+(?:-[a-z0-9_]+)*', ident) or ident in ids:
            raise ValueError(f'Invalid or duplicate scene ID: {ident}')
        ids.add(ident)
        name = f'images/{ident}.png'
        if scene['file'] != name or (scene['width'], scene['height']) != (3840, 2160):
            raise ValueError(f'Invalid 4K image metadata: {ident}')
        data = read_file(source, name)
        if (len(data) < 33 or data[:16] != b'\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR'
                or struct.unpack('>II', data[16:24]) != (3840, 2160)):
            raise ValueError(f'Invalid 4K PNG: {name}')
        if len(data) != scene['bytes'] or hashlib.sha256(data).hexdigest() != scene['sha256']:
            raise ValueError(f'Image checksum mismatch: {name}')
        names.update((name, f'thumbs/{ident}.jpg', f'recipes/{ident}.scene'))
    for start in range(0, len(scenes), 24):
        names.add(f'contact-sheets/{start+1:03}-{min(start+24,len(scenes)):03}.jpg')
    files = {name: read_file(source, name) for name in sorted(names)}
    for name, data in files.items():
        if name.endswith('.jpg') and not (data.startswith(b'\xff\xd8\xff') and data.endswith(b'\xff\xd9')):
            raise ValueError(f'Invalid JPEG: {name}')
    rows = list(csv.DictReader(io.StringIO(files['captions.csv'].decode('utf-8'))))
    if [row['file'] for row in rows] != [scene['file'] for scene in scenes]:
        raise ValueError('Caption catalogue differs from the image manifest')
    links = Links()
    links.feed(files['index.html'].decode('utf-8'))
    if missing := links.paths - names:
        raise ValueError(f'Page links outside the publication list: {sorted(missing)}')
    return files


def stage(source, output):
    files = collect(source)
    destination = output.resolve()
    origin = source.resolve()
    if destination == origin or destination in origin.parents or origin in destination.parents:
        raise ValueError('Staging must be separate from the source gallery')
    if any(part.is_symlink() for part in (output, *output.parents)):
        raise ValueError('Staging path must not contain symlinks')
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        raise ValueError('Use a new or empty staging directory')
    output.mkdir(parents=True, exist_ok=True)
    for name, data in files.items():
        path = output / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    (output / '.nojekyll').write_text('')
    return len(files), sum(map(len, files.values()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'build/gallery-pages')
    args = parser.parse_args()
    try:
        count, size = stage(GALLERY, args.output)
    except (ValueError, OSError, KeyError) as error:
        parser.exit(1, f'Gallery publication failed: {error}\n')
    print(f'Staged {count} gallery files ({size/1024/1024:.2f} MiB) in {args.output}')


if __name__ == '__main__':
    main()
