#!/usr/bin/env python3
"""Emulate button/LED routines in one hash-identified original ZY-M100 MCU image.

Requires Unicorn. Supply the original image separately; no device is accessed.
This validates binary logic, not physical switch wiring or optical LED output.
"""
from pathlib import Path
import argparse
import hashlib,json,struct
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.arm_const import *
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('image',type=Path)
args=p.parse_args()
b=args.image.read_bytes()
if hashlib.sha256(b).hexdigest()!='946831e660267822dde10ec9f02a7486711a90de2b898ef77d7b06737a5f389b':
 p.error('Unsupported original image; function offsets are valid only for the documented SHA256')
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
u.mem_map(0x08000000,0x10000);u.mem_write(0x08000000,b)
u.mem_map(0x20000000,0x10000);u.mem_map(0x48000000,0x1000)
# Fixed function addresses are guarded by the whole-image SHA256 above.
# Button scan 0x3330; time getter 0x3928; UART frame send 0x4694; LED tick 0x3370.
now=0;pressed=0;serial=[];pins=[]
end=0x0800fff0
buffer=struct.unpack_from('<I',b,0x3f78)[0]
def ret(value=None):
 if value is not None:u.reg_write(UC_ARM_REG_R0,value)
 u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
def hook(uc,a,size,ctx):
 if a==0x08002c54:ret(pressed)
 elif a==0x08003928:ret(now)
 elif a==0x08004694:
  cmd=uc.reg_read(UC_ARM_REG_R0);n=uc.reg_read(UC_ARM_REG_R1)
  serial.append((now,cmd,bytes(uc.mem_read(buffer+8,n)).hex()));ret()
 elif a==end:uc.emu_stop()
def write(uc,access,a,size,value,ctx):
 if a in (0x48000018,0x48000028):pins.append((now,a,value))
u.hook_add(UC_HOOK_CODE,hook);u.hook_add(UC_HOOK_MEM_WRITE,write)
def call(a,*args):
 u.reg_write(UC_ARM_REG_SP,0x2000f000);u.reg_write(UC_ARM_REG_LR,end|1)
 for reg,value in zip([UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2],args):u.reg_write(reg,value)
 u.emu_start(a|1,end,count=20000)
 assert u.reg_read(UC_ARM_REG_PC)==end,hex(u.reg_read(UC_ARM_REG_PC))
pressed=1
for now in [100,5099,5100]:call(0x08003330)
assert serial==[],serial
now=5101;call(0x08003330)
assert serial==[(5101,3,'01')],serial
now=9000;call(0x08003330);assert len(serial)==1
pressed=0;call(0x08003330)
pressed=1;now=10000;call(0x08003330);now=15001;call(0x08003330)
assert len(serial)==2
# Stock LED routine uses 1ms SysTick, network state 3 and 500/500 table.
# Establish its normal enabled flag; this is a firmware simulation, no optical observation.
u.mem_write(0x200005a8+0x1c,b'\x01');u.mem_write(0x200000dc,b'\x03')
for now in range(2000):call(0x08003370)
edges=[]
for t,a,v in pins:
 level=int(a==0x48000018)
 if not edges or edges[-1][1]!=level:edges.append((t,level))
assert len(edges)>=4 and all(495<=edges[i][0]-edges[i-1][0]<=505 for i in range(1,len(edges))),edges
print(json.dumps({'binary_sha256':hashlib.sha256(b).hexdigest(),'button_threshold_ms':5001,'single_request_per_hold':True,'new_hold_rearms':True,'pairing_gpio_edges_ms':edges,'test_kind':'stock MCU emulation, not physical button or optical LED observation'}))
