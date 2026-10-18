#!/usr/bin/env python3
"""Download and verify the pinned external Linux build tools."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def install(root, name, item):
    destination = root / item['directory']
    marker = destination / '.archive-sha256'
    if marker.exists() and marker.read_text().strip() == item['sha256']:
        return f'{name}: cached'
    archive = root / item['archive']
    if not archive.exists() or hashlib.file_digest(archive.open('rb'), 'sha256').hexdigest() != item['sha256']:
        subprocess.run(['curl', '--fail', '--location', '--retry', '3', '--silent',
                        '--show-error', '--output', str(archive), item['url']], check=True)
    if hashlib.file_digest(archive.open('rb'), 'sha256').hexdigest() != item['sha256']:
        raise ValueError(f'{name}: archive checksum mismatch')
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir()
    if archive.suffix == '.zip':
        with zipfile.ZipFile(archive) as z:
            for member in z.infolist():
                target = destination / member.filename
                if not target.resolve().is_relative_to(destination.resolve()):
                    raise ValueError('Unsafe archive path')
                z.extract(member, destination)
                mode = member.external_attr >> 16
                if mode and not member.is_dir():
                    target.chmod(mode & 0o777)
    else:
        with tarfile.open(archive) as t:
            t.extractall(destination, filter='data')
    marker.write_text(item['sha256'] + '\n')
    archive.unlink()
    return f'{name}: verified and installed'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--tools', type=Path, default=ROOT / '.tools')
    a = p.parse_args()
    a.tools.mkdir(parents=True, exist_ok=True)
    lock = json.loads((ROOT / 'config/tools.json').read_text())
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        tasks = [pool.submit(install, a.tools, name, item) for name, item in lock.items()]
        for task in concurrent.futures.as_completed(tasks):
            print(task.result(), flush=True)


if __name__ == '__main__':
    main()
