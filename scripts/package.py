#!/usr/bin/env python3
"""Create an unencrypted development GBL and Zigbee OTA from a validated app bin."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

MAGIC=bytes.fromhex('13b779fac925ddb7adf3cfe0f1b614b8')
APP_START=0x4000
SLOT_START=0x60000
SLOT_SIZE=0x56000

def tag(identifier,payload):return struct.pack('<II',identifier,len(payload))+payload

def package(data,version,image_type=0x4d43):
    if not data or APP_START+len(data)>SLOT_START:raise ValueError('Application overlaps OTA staging slot')
    sp,reset=struct.unpack_from('<II',data)
    if not 0x20000000<sp<=0x20010000 or not reset&1 or not APP_START<=reset<SLOT_START:
        raise ValueError('Invalid application vectors')
    offset=data.find(MAGIC)
    if offset<0 or data.find(MAGIC,offset+1)>=0:raise ValueError('Expected one ApplicationProperties structure')
    _,signature,_=struct.unpack_from('<III',data,offset+16)
    info=data[offset+28:offset+56]
    kind,app_version,_=struct.unpack_from('<III',info)
    if signature!=0 or not kind&1 or app_version!=version:raise ValueError('Application signature/type/version mismatch')
    padded=data+b'\xff'*((-len(data))%4)
    gbl=tag(0x03a617eb,struct.pack('<II',0x03000000,0))
    gbl+=tag(0xf40a0af4,info)
    gbl+=tag(0xfe0101fe,struct.pack('<I',APP_START)+padded)
    gbl+=struct.pack('<II',0xfc0404fc,4)
    gbl+=struct.pack('<I',zlib.crc32(gbl))
    if zlib.crc32(gbl)!=0x2144df1c:raise ValueError('GBL CRC residue mismatch')
    payload=struct.pack('<HI',0,len(gbl))+gbl
    header=struct.pack('<IHHHHHIH32sI',0x0beef11e,0x0100,56,0,0xffff,image_type,
                       version,2,b'Zigbee presence firmware'.ljust(32,b'\0'),56+len(payload))
    ota=header+payload
    if len(ota)>SLOT_SIZE:raise ValueError('OTA image does not fit staging slot')
    return gbl,ota

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('application',type=Path);p.add_argument('--version',type=int,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    gbl,ota=package(a.application.read_bytes(),a.version)
    manifest={'version':a.version,'manufacturer':65535,'image_type':0x4d43,'signed':False,'artifacts':[]}
    for name,data in [('bridge.gbl',gbl),('bridge.ota',ota)]:
        (a.output/name).write_bytes(data)
        manifest['artifacts'].append({'file':name,'size':len(data),'sha256':hashlib.sha256(data).hexdigest()})
    (a.output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest))
if __name__=='__main__':main()
