#!/usr/bin/env python3
"""Exact-target SWD operations. Flash writes require a private verified backup."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import tempfile
import time

PAGE=0x2000
PART=0x01150014
BOX=0x20002000

class Debugger:
    def __init__(self, executable='openocd'):
        self.tmp=tempfile.TemporaryDirectory(prefix='mcu-bridge-swd-')
        self.root=Path(self.tmp.name)
        sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
        config=self.root/'debug.cfg'
        config.write_text(f'''source [find interface/jlink.cfg]
transport select swd
adapter speed 100
reset_config none
gdb_port disabled
telnet_port disabled
bindto 127.0.0.1
tcl_port {port}
swd newdap bridge cpu -expected-id 0x6ba02477
dap create bridge.dap -chain-position bridge.cpu
target create bridge.cpu cortex_m -dap bridge.dap
cortex_m reset_config sysresetreq
init
''')
        self.log=(self.root/'openocd.log').open('w+')
        self.process=subprocess.Popen([executable,'-f',str(config)],stdout=self.log,stderr=subprocess.STDOUT)
        for _ in range(50):
            if self.process.poll() is not None:
                self.log.seek(0);raise RuntimeError(self.log.read()[-1800:])
            try:self.sock=socket.create_connection(('127.0.0.1',port),timeout=.2);break
            except OSError:time.sleep(.1)
        else:raise RuntimeError('OpenOCD startup timeout')
        self.sock.settimeout(30)
    def call(self, command):
        # Catch Tcl exceptions explicitly; a successful TCP exchange is not success.
        self.sock.sendall(('set rc [catch {'+command+'} result]; format "%d:%s" $rc $result\x1a').encode())
        out=b''
        while not out.endswith(b'\x1a'):
            chunk=self.sock.recv(65536)
            if not chunk:raise RuntimeError('OpenOCD disconnected')
            out+=chunk
        result=out[:-1].decode();rc,_,value=result.partition(':')
        if rc!='0':raise RuntimeError(f'{command.split()[0]}: {value}')
        return value
    def read(self, address, count=1):
        return [int(x,0) for x in self.call(f'read_memory {address:#x} 32 {count}').split()]
    def dump(self, path, address, size):
        # Small transfers avoid a single long SWD read exceeding the RPC timeout.
        temporary=self.root/'read-chunk.bin'
        with Path(path).open('wb') as output:
            for offset in range(0,size,PAGE):
                length=min(PAGE,size-offset)
                for attempt in range(3):
                    try:
                        self.call(f'dump_image {{{temporary}}} {address+offset:#x} {length:#x}')
                        break
                    except RuntimeError:
                        if attempt==2:raise RuntimeError(f'SWD read failed at {address+offset:#x}')
                        self.call('bridge.dap dpreg 0 0x1e')
                        self.call('poll')
                data=temporary.read_bytes()
                if len(data)!=length:raise RuntimeError('Short SWD chunk')
                output.write(data);output.flush()
                if size>0x10000 and offset%(16*PAGE)==0:print(f'Snapshot read {offset+length}/{size} bytes',flush=True)
        result=Path(path).read_bytes()
        if len(result)!=size:raise RuntimeError('Short SWD read')
        return result
    def close(self):
        try:self.sock.sendall(b'shutdown\x1a')
        except (AttributeError,OSError):pass
        try:self.process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.process.terminate()
            try:self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:self.process.kill();self.process.wait(timeout=3)
        self.log.close()
        logdir=Path(__file__).resolve().parents[1]/'build';logdir.mkdir(exist_ok=True)
        shutil.copyfile(self.root/'openocd.log',logdir/'swd-last.log')
        self.tmp.cleanup()

def normalize_ieee(text):
    return int(text.replace(':','').replace('-',''),16)

def flash(d,args):
    backup=args.backup.resolve()
    saved=(backup/'mainflash-a.bin').read_bytes()
    duplicate=(backup/'mainflash-b.bin').read_bytes()
    if len(saved)!=0xc0000 or saved!=duplicate:raise RuntimeError('Two identical complete main-flash backups required')
    identity=(backup/'devinfo-a.bin').read_bytes()
    if len(identity)<0x50:raise RuntimeError('Missing DEVINFO backup')
    expected=normalize_ieee(args.ieee)
    if struct.unpack_from('<Q',identity,0x48)[0]!=expected:raise RuntimeError('Backup IEEE mismatch')
    part=d.read(0x0fe08004)[0];lo,hi=d.read(0x0fe08048,2)
    if part!=PART or d.read(0x0fe0800c)[0]!=0x00400300 or (hi<<32|lo)!=expected:
        raise RuntimeError('Live chip, memory size or IEEE does not match the verified target')
    boot=args.bootloader.read_bytes();app=args.application.read_bytes();loader=args.loader.read_bytes()
    if not 0<len(boot)<=0x4000 or not 0<len(app)<=0x5c000 or len(loader)>PAGE:
        raise RuntimeError('Image exceeds layout bounds')
    if struct.unpack_from('<I',boot,4)[0]>=0x4000 or not 0x4000<=struct.unpack_from('<I',app,4)[0]<0x60000:
        raise RuntimeError('Unexpected reset vector')
    stamp=time.strftime('%Y%m%dT%H%M%SZ',time.gmtime())
    evidence=backup/f'custom-flash-{stamp}';evidence.mkdir(mode=0o700)
    d.call('halt');d.call('wait_halt 3000')
    # Recoverable RAM changes begin only after a fresh flash snapshot exists.
    d.dump(evidence/'preflash.bin',0,0xc0000)
    d.dump(evidence/'preflash-ram.bin',0x20000000,0x10000)
    d.call(f'load_image {{{args.loader.resolve()}}} 0x20000000 bin')
    d.call('reg primask 1');d.call('reg control 0');d.call('reg basepri 0');d.call('reg faultmask 0')
    d.call('reg xpsr 0x01000000')
    def execute(command,address=0,data=b''):
        payload=struct.pack('<8I',0x464c5348,command,address,len(data),0xffffffff,0,0,0)+data
        image=d.root/'payload.bin';image.write_bytes(payload)
        d.call(f'load_image {{{image}}} {BOX:#x} bin')
        d.call('reg msp 0x2000fff0');d.call('reg sp 0x2000fff0')
        d.call('reg pc 0x20000000');d.call('resume 0x20000000');d.call('wait_halt 5000')
        _,_,_,_,status,target_part,target_lo,target_hi=d.read(BOX,8)
        if status or target_part!=PART or target_hi<<32|target_lo!=expected:
            raise RuntimeError(f'RAM loader failed: status={status:#x}, part={target_part:#x}')
    execute(0)
    print('RAM loader dry run and exact target identity: PASS',flush=True)
    pages={}
    # Install complete pages, padded FF; never touch userdata/DEVINFO/SE storage.
    for base,image in [(0,boot),(0x4000,app)]:
        for offset in range(0,len(image),PAGE):
            pages[base+offset]=image[offset:offset+PAGE].ljust(PAGE,b'\xff')
    if args.initialize_nvm:
        for address in range(0xb6000,0xbe000,PAGE):pages[address]=b'\xff'*PAGE
    if args.clear_ota:
        # Invalidate both staged GBL and the end-of-slot Zigbee OTA metadata.
        # Discard only a superseded/failed download; network NVM starts at B6000.
        for address in [0x60000,0xb4000]:pages[address]=b'\xff'*PAGE
    report={'stock_sha256':hashlib.sha256(saved).hexdigest(),'pages':[]}
    for index,(address,data) in enumerate(sorted(pages.items())):
        execute(2 if data==b'\xff'*PAGE else 1,address,b'' if data==b'\xff'*PAGE else data)
        actual=d.dump(evidence/'page-readback.bin',address,PAGE)
        if actual!=data:raise RuntimeError(f'Flash readback mismatch at {address:#x}; target left halted for recovery')
        report['pages'].append({'address':address,'sha256':hashlib.sha256(actual).hexdigest()})
        if index%8==0 or index+1==len(pages):print(f'Verified pages {index+1}/{len(pages)}',flush=True)
    (evidence/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    d.call('reset run')
    print('Targeted flash, page readback and reset: PASS',flush=True)
    print(f'Private evidence: {evidence}',flush=True)

def main():
    os.umask(0o077)
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--openocd',default='openocd')
    sub=p.add_subparsers(dest='action',required=True)
    sub.add_parser('identify')
    read=sub.add_parser('read');read.add_argument('address',type=lambda x:int(x,0));read.add_argument('count',type=int,nargs='?',default=1)
    cmd=sub.add_parser('command');cmd.add_argument('tcl')
    f=sub.add_parser('flash')
    for name in ['backup','loader','bootloader','application']:f.add_argument('--'+name,type=Path,required=True)
    f.add_argument('--ieee',required=True);f.add_argument('--initialize-nvm',action='store_true')
    f.add_argument('--clear-ota',action='store_true',help='Invalidate superseded staged image; preserve network NVM')
    args=p.parse_args();d=Debugger(args.openocd)
    try:
        if args.action=='flash':flash(d,args)
        elif args.action=='identify':print({'part':hex(d.read(0x0fe08004)[0]),'ieee_words':[hex(x) for x in d.read(0x0fe08048,2)]})
        elif args.action=='read':print([hex(x) for x in d.read(args.address,args.count)])
        else:print(d.call(args.tcl))
    finally:d.close()

if __name__=='__main__':main()
