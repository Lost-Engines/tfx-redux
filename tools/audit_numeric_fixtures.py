#!/usr/bin/env python3
"""Measure portable-arithmetic rounding against unchanged reference fixtures."""
import argparse
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=Path('build'))
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    build = args.build.resolve()
    root = Path(__file__).resolve().parents[1]
    probes = {p.stem.removesuffix('_probe') + '_probe' for p in (root / 'tests').glob('*_probe.cpp')
              if '#include "numeric_fixture.h"' in p.read_text()}
    tests = json.loads(subprocess.check_output(
        ['ctest', '--test-dir', str(build), '--show-only=json-v1'], text=True))['tests']
    reports = []
    with tempfile.TemporaryDirectory(prefix='tfx-numeric-audit-') as temporary:
        for test in tests:
            if Path(test['command'][0]).stem not in probes:
                continue
            output = Path(temporary) / (test['name'] + '.txt')
            environment = os.environ.copy()
            environment.pop('TFX_RECORD_TRIG_FIXTURE', None)
            environment['TFX_NUMERIC_AUDIT'] = str(output)
            subprocess.run(test['command'], cwd=build, env=environment, check=True, capture_output=True, text=True)
            lines = output.read_text().splitlines() if output.exists() else []
            largest_absolute = largest_scaled_eps = 0.0
            for line in lines:
                offset, width, expected, actual = map(int, line.split())
                integer, real, epsilon = ('<I', '<f', 2**-23) if width == 32 else ('<Q', '<d', 2**-52)
                expected = struct.unpack(real, struct.pack(integer, expected))[0]
                actual = struct.unpack(real, struct.pack(integer, actual))[0]
                if math.isnan(expected) and math.isnan(actual):
                    continue
                if not (math.isfinite(actual) and math.isfinite(expected)):
                    raise RuntimeError(f"{test['name']}: nonfinite mismatch at byte {offset}")
                error = abs(expected - actual)
                largest_absolute = max(largest_absolute, error)
                largest_scaled_eps = max(largest_scaled_eps, error / max(1, abs(actual), abs(expected)) / epsilon)
            report = dict(test=test['name'], changed_floating_fields=len(lines),
                          maximum_absolute_error=largest_absolute, maximum_scaled_epsilon=largest_scaled_eps)
            reports.append(report)
            print(f"{test['name']}: {len(lines)} rounding differences; "
                  f"max |error| {largest_absolute:.6g}, scaled error {largest_scaled_eps:.6g} epsilon")
    if not reports:
        raise RuntimeError('No numeric fixture tests found; configure BUILD_TESTING=ON first')
    if args.report:
        args.report.write_text(json.dumps(reports, indent=2) + '\n')


if __name__ == '__main__':
    main()
