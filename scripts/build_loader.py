#!/usr/bin/env python3
"""Build the RAM programmer against the same external SDK as the application."""
import argparse
from pathlib import Path
import subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--tools',type=Path,required=True);a=p.parse_args()
    root=Path(__file__).resolve().parents[1];tools=a.tools.resolve();sdk=tools/'sdk'
    prefix=tools/'gcc/arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi/bin/arm-none-eabi-'
    out=root/'build/loader';out.mkdir(parents=True,exist_ok=True)
    args=[str(prefix)+'gcc','-mcpu=cortex-m33','-mthumb','-Os','-g','-ffreestanding',
          '-ffunction-sections','-fdata-sections','-DNDEBUG','-DEFR32MG21A020F768IM32','-nostartfiles','-nostdlib']
    for path in ['platform/Device/SiliconLabs/EFR32MG21/Include','platform/CMSIS/Core/Include','platform/common/inc','platform/emlib/inc']:
        args+=['-I',str(sdk/path)]
    args+=[str(root/'scripts/flash_loader.c'),str(sdk/'platform/emlib/src/em_msc.c'),
           '-T',str(root/'scripts/flash_loader.ld'),'-Wl,--gc-sections','-o',str(out/'loader.elf')]
    subprocess.run(args,check=True)
    subprocess.run([str(prefix)+'objcopy','-O','binary',str(out/'loader.elf'),str(out/'loader.bin')],check=True)
    undefined=subprocess.check_output([str(prefix)+'nm','-u',str(out/'loader.elf')],text=True)
    if undefined:raise SystemExit('Unexpected unresolved loader symbols: '+undefined)
    print('RAM loader built; no unresolved symbols; bytes:',(out/'loader.bin').stat().st_size)
if __name__=='__main__':main()
