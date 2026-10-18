#!/usr/bin/env python3
"""SWD-only diagnostics for an explicitly built --debug-swd firmware image."""
import argparse
from pathlib import Path
import subprocess
import time
from swd import Debugger, normalize_ieee

FIELDS=['magic','version','command','status','endpoint','dp','type','value','enabled',
        'delay','active','clear','ticks','frames','queued','delivered','failed','network']
QUERY=['query_received','query_value','query_source','query_source_ep','query_destination_ep','send_index']

class Mailbox:
    def __init__(self,debugger,elf,nm):
        symbols=subprocess.check_output([nm,'-S','--defined-only',str(elf)],text=True)
        found=[line.split() for line in symbols.splitlines() if line.endswith(' bridge_debug')]
        if len(found)!=1:raise RuntimeError('No diagnostic mailbox in this ELF; use an exact --debug-swd image')
        self.address=int(found[0][0],16);self.size=int(found[0][1],16);self.d=debugger
        if self.d.read(self.address,2)!=[0x4d435542,1]:raise RuntimeError('Live mailbox does not match this ELF')
    def status(self):
        fields=FIELDS+(QUERY if self.size>=192 else [])
        return dict(zip(fields,self.d.read(self.address,len(fields))))
    def run(self,command,**values):
        if self.d.read(self.address+8)[0]:raise RuntimeError('A previous command is still pending')
        for field,value in values.items():
            self.d.call(f'mww {self.address+4*FIELDS.index(field):#x} {int(value)}')
        self.d.call(f'mww {self.address+8:#x} {command}')
        deadline=time.monotonic()+3
        while time.monotonic()<deadline:
            result=self.status()
            if not result['command']:
                if result['status']!=1:raise RuntimeError(f'Diagnostic command failed: {result["status"]}')
                return result
            time.sleep(.01)
        raise RuntimeError('Diagnostic command timed out')
    def query(self,endpoint):
        if self.size<192:raise RuntimeError('This diagnostic build predates direct bound-state reads')
        before=self.status()['query_received'];self.run(4,endpoint=endpoint)
        deadline=time.monotonic()+3
        while time.monotonic()<deadline:
            result=self.status()
            if result['query_received']!=before:return {k:result[k] for k in QUERY[:5]}
            time.sleep(.025)
        raise RuntimeError('No fresh On/Off read response from the bound actuator')

def uint32(text):
    value=int(text,0)
    if not 0<=value<=0xffffffff:raise argparse.ArgumentTypeError('Expected uint32')
    return value

def bounded(maximum):
    def parse(text):
        value=int(text,0)
        if not 0<=value<=maximum:raise argparse.ArgumentTypeError(f'Expected 0..{maximum}')
        return value
    return parse

def main():
    import json
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--elf',type=Path,required=True);p.add_argument('--nm',default='arm-none-eabi-nm')
    p.add_argument('--openocd',default='openocd');p.add_argument('--expect-ieee',required=True)
    sub=p.add_subparsers(dest='action',required=True)
    sub.add_parser('status');sub.add_parser('pair')
    q=sub.add_parser('query');q.add_argument('--endpoint',type=int,choices=[1],required=True)
    i=sub.add_parser('inject');i.add_argument('--dp',type=bounded(255),required=True)
    i.add_argument('--type',type=int,choices=[1,2,4,5],required=True);i.add_argument('--value',type=uint32,required=True)
    r=sub.add_parser('route');r.add_argument('--endpoint',type=int,choices=[1],required=True)
    r.add_argument('--dp',type=bounded(255),required=True);r.add_argument('--type',type=int,choices=[1,2,4,5],required=True)
    r.add_argument('--active',type=uint32,default=1);r.add_argument('--clear',type=uint32,default=0)
    r.add_argument('--delay',type=bounded(65535),default=0);r.add_argument('--enabled',type=int,choices=[0,1],required=True)
    a=p.parse_args();d=Debugger(a.openocd)
    try:
        low,high=d.read(0x0fe08048,2)
        if high<<32|low!=normalize_ieee(a.expect_ieee):raise RuntimeError('Wrong physical target IEEE')
        m=Mailbox(d,a.elf,a.nm)
        if a.action=='status':result=m.status()
        elif a.action=='pair':result=m.run(1)
        elif a.action=='inject':result=m.run(2,dp=a.dp,type=a.type,value=a.value)
        elif a.action=='query':result=m.query(a.endpoint)
        else:result=m.run(3,endpoint=a.endpoint,dp=a.dp,type=a.type,active=a.active,clear=a.clear,delay=a.delay,enabled=a.enabled)
        print(json.dumps(result))
    finally:d.close()

if __name__=='__main__':main()
