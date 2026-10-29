#!/usr/bin/env python3
"""Audit changed trig expectations; --record explicitly accepts new outputs.

Every input token is preserved. The independent geometry tests must pass first.
Recorded outputs are regression baselines, not independent correctness evidence.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=root / 'build')
parser.add_argument('--record', action='store_true')
args = parser.parse_args()
build = args.build.resolve()
manifest_path = root / 'tests/trig_fixture_manifest.json'
manifest = json.loads(manifest_path.read_text())
environment = os.environ.copy()
environment.pop('TFX_RECORD_TRIG_FIXTURE', None)
for test in ('trigonometry_tests', 'continuous_pose_tests'):
    subprocess.run([str(build / test)], check=True, env=environment)
changes = {}
originals = {}
with tempfile.TemporaryDirectory(prefix='trig-expectations-', dir=build) as temporary:
    for index, entry in enumerate(manifest):
        fixture = root / entry['fixture']
        original = originals.setdefault(entry['fixture'], fixture.read_bytes())
        digest = hashlib.sha256(original).hexdigest()
        if digest not in (entry['original_sha256'], entry.get('redux_sha256')):
            raise RuntimeError(f"Fixture changed since review: {entry['fixture']}")
        report = Path(temporary) / f'{index}.txt'
        env = environment | {'TFX_RECORD_TRIG_FIXTURE': str(report)}
        command = [str(build / entry['command'][0])]
        command += [str(fixture) if x == '@fixture' else x for x in entry['command'][1:]]
        result = subprocess.run(command, env=env, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(f"{entry['test']}: {result.stdout}{result.stderr}")
        lines = report.read_text().splitlines() if report.exists() else []
        patches = changes.setdefault(entry['fixture'], {})
        for line in lines:
            begin, end, old, new = line.split()
            begin, end = int(begin), int(end)
            if int(original[begin:end]) != int(old):
                raise RuntimeError(f"Expectation offset changed: {entry['fixture']}: {line}")
            key, replacement = (begin, end), new.encode('ascii')
            if key in patches and patches[key] != replacement:
                raise RuntimeError(f"Probe modes disagree: {entry['fixture']}: {line}")
            patches[key] = replacement
        print(f"{entry['test']}: {len(lines)} changed expectations")

if args.record:
    for filename, patches in changes.items():
        data = originals[filename]
        for (begin, end), replacement in sorted(patches.items(), reverse=True):
            data = data[:begin] + replacement + data[end:]
        (root / filename).write_bytes(data)
    for entry in manifest:
        entry['redux_sha256'] = hashlib.sha256((root / entry['fixture']).read_bytes()).hexdigest()
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    print('Recorded Redux expectations; original inputs preserved.')
else:
    total = sum(len(patches) for patches in changes.values())
    print(f'{total} changed expected tokens; no fixtures written.')
    raise SystemExit(1 if total else 0)
