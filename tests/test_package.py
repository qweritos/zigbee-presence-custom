"""Format and safety checks using a synthetic image, never a device dump."""
import importlib.util
from pathlib import Path
import struct
import unittest
import zlib

spec=importlib.util.spec_from_file_location('bridge_package',Path(__file__).resolve().parents[1]/'scripts/package.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

def image():
    data=bytearray(b'\xff'*1024)
    struct.pack_into('<II',data,0,0x20001000,0x4101)
    data[128:184]=module.MAGIC+struct.pack('<6I',0x102,0,0xffffffff,1,2,0)+bytes(16)
    return bytes(data)

class PackageTests(unittest.TestCase):
    def test_standard_headers_and_payload(self):
        app=image();gbl,ota=module.package(app,2)
        self.assertEqual(struct.unpack_from('<IHHHHHIH',ota),
                         (0x0beef11e,0x100,56,0,0xffff,0x4d43,2,2))
        self.assertEqual(struct.unpack_from('<I',ota,52)[0],len(ota))
        self.assertEqual(struct.unpack_from('<HI',ota,56),(0,len(gbl)))
        self.assertEqual(ota[62:],gbl)
        tags=[];offset=0
        while offset<len(gbl):
            identifier,length=struct.unpack_from('<II',gbl,offset);offset+=8
            payload=gbl[offset:offset+length];self.assertEqual(len(payload),length)
            tags.append((identifier,payload));offset+=length
        self.assertEqual([x[0] for x in tags],[0x03a617eb,0xf40a0af4,0xfe0101fe,0xfc0404fc])
        self.assertEqual(struct.unpack('<II',tags[0][1]),(0x03000000,0))
        self.assertEqual(struct.unpack_from('<I',tags[2][1])[0],0x4000)
        self.assertEqual(tags[2][1][4:],app)
        self.assertEqual(zlib.crc32(gbl),0x2144df1c)
        corrupt=bytearray(gbl);corrupt[100]^=1
        self.assertNotEqual(zlib.crc32(corrupt),0x2144df1c)

    def test_version_signature_vector_and_layout_guards(self):
        with self.assertRaises(ValueError):module.package(image(),3)
        for offset,value in [(0,0x30000000),(4,0x4100),(4,0x60001),(148,1)]:
            bad=bytearray(image());struct.pack_into('<I',bad,offset,value)
            with self.assertRaises(ValueError):module.package(bad,2)
        with self.assertRaises(ValueError):module.package(image()+module.MAGIC,2)
        with self.assertRaises(ValueError):module.package(image().ljust(0x5c004,b'\xff'),2)
        # An app can fit executable flash yet exceed the smaller OTA staging slot.
        with self.assertRaises(ValueError):module.package(image().ljust(0x56000,b'\xff'),2)

if __name__=='__main__':unittest.main()
