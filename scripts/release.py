#!/usr/bin/env python3
"""Prepare per-model release artifacts and a combined public Zigpy OTA index."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import zipfile
from version import release_version

ROOT = Path(__file__).resolve().parents[1]


def metadata(tag=None):
    version = (ROOT / 'VERSION').read_text().strip()
    _, encoded = release_version(version)
    if tag and tag != 'v' + version:
        raise ValueError(f'Tag {tag} does not match VERSION {version}')
    models = json.loads((ROOT / 'config/models.json').read_text())
    return version, encoded, models


def stage(profile, output, repository, tools):
    version, encoded, models = metadata()
    if profile not in models:
        raise ValueError('Unsupported model profile')
    build = ROOT / 'build' / profile
    manifest = json.loads((build / 'build-manifest.json').read_text())
    if manifest['version'] != encoded or manifest['release_version'] != version or manifest['debug_swd']:
        raise ValueError('Build is not this production release')
    for item in manifest['artifacts']:
        if hashlib.sha256((ROOT / item['file']).read_bytes()).hexdigest() != item['sha256']:
            raise ValueError('Build artifact checksum mismatch')
    from package import package
    app = (build / 'bridge.bin').read_bytes()
    # The ZCL length byte is initialized by bridge_init, so it is zero on disk.
    if b'\0' + version.encode() + b'\0' not in app:
        raise ValueError('Basic SW Build ID does not match release version')
    gbl, ota = package(app, encoded)
    if gbl != (build / 'ota/bridge.gbl').read_bytes() or ota != (build / 'ota/bridge.ota').read_bytes():
        raise ValueError('Packaged image differs from the compiled application')
    if struct.unpack_from('<I', ota, 14)[0] != encoded:
        raise ValueError('OTA header version mismatch')
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise ValueError('Release staging directory must be empty')
    sources = {
        f'{profile}.bin': build / 'bridge.bin',
        f'{profile}.elf': build / 'generated/build/release/mcu_bridge.out',
        f'{profile}.gbl': build / 'ota/bridge.gbl',
        f'{profile}.ota': build / 'ota/bridge.ota',
        f'{profile}-bootloader.bin': ROOT / 'build/bootloader-release/bootloader.bin',
        f'{profile}-loader.bin': ROOT / 'build/loader/loader.bin',
        'zigbee_presence.py': ROOT / 'quirks/zigbee_presence.py',
        'LICENSE': ROOT / 'LICENSE',
        'NOTICE': ROOT / 'NOTICE',
        'upstream-license.txt': ROOT / 'LICENSES/tuya-zigbee-switch.txt',
        'SiliconLabs-SDK-License.txt': tools / 'sdk/License.txt',
        'README.md': ROOT / 'docs/releasing.md',
        'hardware.md': ROOT / 'docs/hardware.md',
    }
    for name, source in sources.items():
        shutil.copyfile(source, output / name)
    boot = (output / f'{profile}-bootloader.bin').read_bytes()
    sp, reset = struct.unpack_from('<II', boot)
    if len(boot) > 0x4000 or not 0x20000000 < sp <= 0x20010000 or not reset & 1 or reset >= 0x4000:
        raise ValueError('Invalid bootloader layout')
    index = json.loads((build / 'ota/index.json').read_text())
    item = index['firmwares'][0]
    item.pop('path')
    item['url'] = f'https://github.com/{repository}/releases/download/v{version}/{profile}.ota'
    (output / f'{profile}-index.json').write_text(json.dumps(index, indent=2) + '\n')
    files = [{'file': p.name, 'size': p.stat().st_size,
              'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
             for p in sorted(output.iterdir()) if p.is_file()]
    release = {'version': version, 'ota_version': encoded, 'profile': profile,
               'model': models[profile]['model'], 'part': models[profile]['part'],
               'commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
               'signed_images': False, 'build': manifest, 'artifacts': files}
    (output / f'{profile}-manifest.json').write_text(json.dumps(release, indent=2) + '\n')
    sums = ''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n'
                   for p in sorted(output.iterdir()) if p.is_file())
    (output / f'{profile}.SHA256SUMS').write_text(sums)
    with zipfile.ZipFile(output / f'{profile}-{version}.zip', 'w', zipfile.ZIP_DEFLATED) as z:
        for p in sorted(output.iterdir()):
            if p.is_file() and p.suffix != '.zip':
                z.write(p, p.name)
    print(f'{profile}: release {version}, OTA 0x{encoded:08x}, production artifacts validated')


def collect(root):
    version, encoded, models = metadata()
    combined = {'firmwares': []}
    for profile in models:
        manifest = json.loads((root / f'{profile}-manifest.json').read_text())
        if manifest['version'] != version or manifest['ota_version'] != encoded:
            raise ValueError('Mixed release versions')
        for item in manifest['artifacts']:
            data = (root / item['file']).read_bytes()
            if len(data) != item['size'] or hashlib.sha256(data).hexdigest() != item['sha256']:
                raise ValueError('Release artifact checksum mismatch')
        combined['firmwares'].extend(json.loads((root / f'{profile}-index.json').read_text())['firmwares'])
    (root / 'index.json').write_text(json.dumps(combined, indent=2) + '\n')
    (root / 'SHA256SUMS').write_text(''.join(
        f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n'
        for p in sorted(root.iterdir()) if p.is_file() and p.name != 'SHA256SUMS'))
    print(f'Validated {len(models)} supported model(s) for release {version}')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='command', required=True)
    m = sub.add_parser('metadata'); m.add_argument('--tag')
    s = sub.add_parser('stage'); s.add_argument('--profile', required=True)
    s.add_argument('--output', type=Path, required=True)
    s.add_argument('--repository', required=True)
    s.add_argument('--tools', type=Path, default=ROOT / '.tools')
    c = sub.add_parser('collect'); c.add_argument('--directory', type=Path, required=True)
    a = p.parse_args()
    if a.command == 'metadata':
        version, encoded, models = metadata(a.tag)
        print(json.dumps({'version': version, 'ota_version': encoded, 'profiles': list(models)}))
    elif a.command == 'stage':
        stage(a.profile, a.output, a.repository, a.tools)
    else:
        collect(a.directory)


if __name__ == '__main__':
    main()
