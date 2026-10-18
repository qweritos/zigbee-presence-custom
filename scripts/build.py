#!/usr/bin/env python3
"""Generate and build the EFR32MG21 radio image with external, pinned tools."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--tools', type=Path, default=Path(os.environ.get('BRIDGE_TOOLS', ROOT / '.tools')))
    p.add_argument('--profile', choices=['generic', 'ztc6ggyl'], default='generic')
    p.add_argument('--force-generation', action='store_true', help='Diagnostic SLC override; does not bypass compilation')
    p.add_argument('--generate-only', action='store_true')
    p.add_argument('--debug-swd', action='store_true', help='Include RAM-only pairing/test mailbox')
    p.add_argument('--version', type=int, default=1, help='Monotonic Zigbee OTA version (uint32)')
    p.add_argument('--bootloader', action='store_true', help='Build the initial wired-conversion bootloader')
    args = p.parse_args()
    if not 1 <= args.version <= 0xffffffff:
        p.error('--version must be 1..4294967295')
    tools = args.tools.resolve()
    sdk = tools / 'sdk'
    slc = tools / 'slc/slc_cli/slc'
    gcc = tools / 'gcc/arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi'
    for required in [sdk/'simplicity_sdk.slcs', slc, gcc/'bin/arm-none-eabi-gcc', tools/'zap/zap-cli']:
        if not required.exists():
            p.error(f'Missing {required}; see docs/build.md')
    out = ROOT / 'build' / ('bootloader-release' if args.bootloader else args.profile)
    out.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    if env.get('JAVA_HOME'):
        env['PATH'] = str(Path(env['JAVA_HOME'])/'bin') + os.pathsep + env['PATH']
    env['STUDIO_ADAPTER_PACK_PATH'] = str(tools/'zap')
    env['CUSTOM_ZCL_XML'] = str(ROOT/'config/custom-zcl.xml')
    env['ZAP_TEMPSTATE'] = '1'  # Isolate SQLite state between project generations.
    config = out/'slc.config'
    def run(command, name):
        log = out/(name+'.log')
        with log.open('w') as f:
            result = subprocess.run([str(x) for x in command], env=env, cwd=ROOT, stdout=f, stderr=subprocess.STDOUT)
        if result.returncode:
            print('\n'.join(log.read_text(errors='replace').splitlines()[-22:]))
            raise SystemExit(f'{name} failed ({result.returncode}); log: {log}')
        print(f'{name}: OK ({log.relative_to(ROOT)})')
    run([slc,'configuration','--sdk',sdk,'--gcc-toolchain',gcc,'--cli-config',config], 'configure')
    run([slc,'signature','trust','--sdk',sdk,'--cli-config',config], 'trust-sdk')
    if args.bootloader:
        generated=out/'generated'
        if generated.is_symlink():p.error('Refusing symlinked build directory')
        if generated.exists():shutil.rmtree(generated)
        command=[slc,'generate','--cli-config',config,'--sdk',sdk,'--project-file',ROOT/'config/bootloader.slcp',
                 '-np','-nocp','-d',generated,'-name','bootloader','--with','EFR32MG21A020F768IM32',
                 '--toolchain=gcc','--output-type=makefile']
        if args.force_generation:command.insert(2,'--force')
        run(command,'generate')
        if args.generate_only:return
        run(['make','-C',generated,'-f','bootloader.Makefile','-j',str(min(os.cpu_count() or 2,8)),'release'],'compile')
        elf=generated/'build/release/bootloader.out';binary=out/'bootloader.bin'
        run([gcc/'bin/arm-none-eabi-objcopy','-O','binary',elf,binary],'binary')
        data=binary.read_bytes();sp,reset=struct.unpack_from('<II',data)
        if len(data)>0x4000 or not 0x20000000<sp<=0x20010000 or not reset&1 or reset>=0x4000:
            raise SystemExit('Bootloader does not fit the verified 16 KiB layout')
        (out/'build-manifest.json').write_text(json.dumps({'target':'EFR32MG21A020F768IM32',
            'sdk':'2025.6.2','generation_forced':args.force_generation,'size':len(data),
            'sha256':hashlib.sha256(data).hexdigest(),'slot_start':0x60000,'slot_size':0x56000},indent=2)+'\n')
        print('Bootloader build and layout checks complete. No hardware was written.')
        return
    text = (ROOT/'config/bridge.slcp').read_text()
    text = re.sub(r'path: ([^},\n]+)', lambda m: 'path: '+str((ROOT/'config'/m[1].strip()).resolve()), text)
    if args.profile == 'ztc6ggyl':
        text += '  - name: BRIDGE_PROFILE_ZTC6GGYL\n    value: "1"\n'
    if args.debug_swd:
        text += '  - name: BRIDGE_ENABLE_DEBUG\n    value: "1"\n'
    text=text.replace('FIRMWARE_VERSION, value: 1}',f'FIRMWARE_VERSION, value: {args.version}'+'}')
    text=text.replace('SL_APPLICATION_VERSION, value: 1}',f'SL_APPLICATION_VERSION, value: {args.version}'+'}')
    project = out/'bridge.slcp'
    project.write_text(text)
    # SLC transfers config-header ownership to the generated project. Reusing it
    # silently retains older version/calibration macros despite SLCP changes.
    generated=out/'generated'
    if generated.is_symlink():
        p.error('Refusing to replace a symlinked generated build directory')
    if generated.exists():
        shutil.rmtree(generated)
    command=[slc,'generate','--cli-config',config,'--sdk',sdk,'--project-file',project,
             '-np','-nocp','-d',out/'generated','-name','bridge','--with','EFR32MG21A020F768IM32',
             '--toolchain=gcc','--output-type=makefile']
    if args.force_generation:
        command.insert(2,'--force')
    run(command,'generate')
    if args.generate_only:
        return
    run(['make','-C',out/'generated','-f','mcu_bridge.Makefile','-j',str(min(os.cpu_count() or 2,8)),'release'],'compile')
    elf=out/'generated/build/release/mcu_bridge.out'
    binary=out/'bridge.bin'
    run([gcc/'bin/arm-none-eabi-objcopy','-O','binary',elf,binary],'binary')
    # The package tool inspects compiled properties and rejects a stale version,
    # invalid vectors, application/staging overlap, or an oversized OTA image.
    run([sys.executable,ROOT/'scripts/package.py',binary,'--version',str(args.version),
         '--output',out/'ota'],'package')
    ota=out/'ota/bridge.ota';data=ota.read_bytes()
    model='TuyaMCU-Bridge-ZTC6' if args.profile=='ztc6ggyl' else 'TuyaMCU-Bridge'
    index={'firmwares':[{'file_version':args.version,'file_size':len(data),
        'manufacturer_id':65535,'image_type':19779,'path':'bridge.ota',
        'checksum':'sha3-256:'+hashlib.sha3_256(data).hexdigest(),
        'manufacturer_names':['OpenZigbee-MCU'],'model_names':[model]}]}
    (out/'ota/index.json').write_text(json.dumps(index,indent=2)+'\n')
    artifacts=[]
    for file in [elf,binary,out/'ota/bridge.gbl',ota]:
        artifacts.append({'file':str(file.relative_to(ROOT)),'sha256':hashlib.sha256(file.read_bytes()).hexdigest()})
    (out/'build-manifest.json').write_text(json.dumps({'profile':args.profile,'sdk':'2025.6.2',
        'version':args.version,'debug_swd':args.debug_swd,
        'generation_forced':args.force_generation,'artifacts':artifacts},indent=2)+'\n')
    print('Build complete. This script never flashes hardware.')

if __name__ == '__main__':
    main()
