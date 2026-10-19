#!/usr/bin/env python3
"""Independently check Redux's corrected distant-point projection fixtures.

Keep native polygon/LOD decisions from the original-code captures; calculate
point pixels with signed integer division, the selected aperture and per-row
screen clipping. No game data or C++ rasterizer is used to calculate hashes.
--record explicitly refreshes the two Redux fixtures; the native ones stay intact.
"""
import argparse
from functools import lru_cache
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=Path('build'))
parser.add_argument('--record', action='store_true')
args = parser.parse_args()
reference = Path(__file__).resolve().parent.parent / 'tests' / 'reference'

def divide(numerator, denominator):
    return (1 if numerator >= 0 else -1) * (abs(numerator) // denominator)

def word(value):
    return (value + 32768) % 65536 - 32768

@lru_cache(maxsize=None)
def digest(points, colour):
    pixels = bytearray([99]) * 64000
    for at in points:
        pixels[at] = colour
    result = 14695981039346656037
    for value in pixels:
        result = ((result ^ value) * 1099511628211) & ((1 << 64) - 1)
    return result

def point_hash(wide, size, colour, ox, oy, minimum_x, minimum_y, maximum_x, maximum_y, x, y, z):
    depth = z >> 16
    points = []
    if depth > 16:
        shift = 9 if wide else 8
        qx, qy = divide(x >> shift, depth), divide(y >> shift, depth)
        assert -32768 <= qx <= 32767 and -32768 <= qy <= 32767
        px, py = word(qx + ox), word(qy + oy)
        if minimum_x <= px <= maximum_x and minimum_y <= py <= maximum_y:
            points = [(py + dy) * 320 + px + dx for dy in range(size) for dx in range(size) if 0 <= px + dx < 320 and 0 <= py + dy < 200]
    return digest(tuple(points), colour)

for native, redux, executable in (('model_points.txt', 'redux_model_points.txt', 'model_point_probe'), ('model_lod.txt', 'redux_model_lod.txt', 'model_lod_probe')):
    rows = []
    for line in (reference / native).read_text().splitlines():
        row = list(map(int, line.split()))
        if native == 'model_points.txt':
            row[-1] = point_hash(*row[:-1])
        else:
            wide, x, y, z, detail, _ = row
            if detail >= 2:
                row[-1] = point_hash(wide, 2 if detail == 2 else 1, 200 if detail == 2 else 201, 160, 100, 0, 0, 319, 199, x, y, z)
        rows.append(' '.join(map(str, row)))
    expected = '\n'.join(rows) + '\n'
    target = reference / redux
    if args.record:
        target.write_text(expected)
    assert target.read_text() == expected, f'{redux} disagrees with the independent projection calculation'
    subprocess.run([str((args.build / executable).resolve()), str(target)], check=True)
    print(f'{redux}: {len(rows)} independent cases passed')
