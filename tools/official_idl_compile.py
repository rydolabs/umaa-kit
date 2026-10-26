#!/usr/bin/env python3
"""Build only the official UMAA IDL transitive TypeSupport closure.

External AUVSI input must be supplied by the user/CI. Never copies or uploads
official source IDLs into the repository. idlc is the authoritative IDL parser.
"""
import argparse
import pathlib
import re
import subprocess

INCLUDE = re.compile(r'^\s*#\s*include\s*["<]([^">]+)[">]', re.MULTILINE)


def collect(root: pathlib.Path, target: pathlib.Path) -> list[pathlib.Path]:
    visiting: set[pathlib.Path] = set()
    visited: set[pathlib.Path] = set()
    result = []

    def walk(path: pathlib.Path):
        path = path.resolve()
        if path in visited:
            return
        if path in visiting:
            raise ValueError(f'IDL include cycle: {path}')
        if root.resolve() not in path.parents:
            raise ValueError(f'IDL include outside root: {path}')
        if not path.is_file():
            raise FileNotFoundError(path)
        visiting.add(path)
        for name in INCLUDE.findall(path.read_text(encoding='utf-8-sig', errors='replace')):
            candidate = root / name
            if not candidate.is_file():
                candidate = path.parent / name
            if not candidate.is_file():
                raise FileNotFoundError(f'Missing {name} imported by {path}')
            walk(candidate)
        visiting.remove(path)
        visited.add(path)
        result.append(path)

    walk(target)
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', type=pathlib.Path, required=True)
    ap.add_argument('--source', default='UMAA/EO/BatteryStatus/BatteryReportType.idl')
    ap.add_argument('--out', type=pathlib.Path, required=True)
    ap.add_argument('--cc', default='cc')
    ap.add_argument('--idlc', default='idlc')
    ap.add_argument('--generate-only', action='store_true', help='Let CMake/MSVC compile the generated source')
    args = ap.parse_args()
    root = args.root.resolve()
    out = args.out.resolve()
    source = pathlib.Path(args.source)
    closure = collect(root, root / source)
    for item in closure:
        parent = out / item.relative_to(root).parent
        parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([args.idlc, '-I', str(root), '-o', str(parent), str(item)], check=True)
    generated_c = out / source.with_suffix('.c')
    generated_h = out / source.with_suffix('.h')
    if not generated_c.is_file() or not generated_h.is_file():
        raise FileNotFoundError(f'Expected generated code missing: {source}')
    if not args.generate_only:
        obj = out / (source.stem + '.o')
        subprocess.run([args.cc, '-std=c11', '-fPIC', '-I', str(out),
                        '-I', str(root), '-c', str(generated_c),
                        '-o', str(obj)], check=True)
        print('OFFICIAL_UMAA_TYPESUPPORT_COMPILED', args.source,
              'transitive_idls=', len(closure), 'object_bytes=', obj.stat().st_size)
    else:
        print('OFFICIAL_UMAA_TYPESUPPORT_GENERATED', args.source,
              'transitive_idls=', len(closure), 'compiler=', args.idlc)


if __name__ == '__main__':
    main()
