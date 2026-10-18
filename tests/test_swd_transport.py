"""Readback must survive a transient parity/poll error and reject failed reads."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('swd',Path(__file__).resolve().parents[1]/'scripts/swd.py')
swd=importlib.util.module_from_spec(spec);spec.loader.exec_module(swd)

class ReadbackTests(unittest.TestCase):
    def test_transient_read_and_poll_failure_retries_verified_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            d=object.__new__(swd.Debugger);d.root=Path(temporary)
            reads=[]
            def call(command):
                if command.startswith('dump_image'):
                    reads.append(command)
                    if len(reads)==1:raise RuntimeError('SWD parity mismatch')
                    (d.root/'read-chunk.bin').write_bytes(b'verified')
                elif command=='poll' or command.startswith('bridge.dap'):
                    raise RuntimeError('transient recovery failure')
                return ''
            d.call=call
            self.assertEqual(d.dump(d.root/'result.bin',0x4000,8),b'verified')
            self.assertEqual(len(reads),2)

    def test_persistent_read_failure_is_never_accepted(self):
        with tempfile.TemporaryDirectory() as temporary:
            d=object.__new__(swd.Debugger);d.root=Path(temporary)
            def call(command):
                if command.startswith('dump_image') or command=='poll':raise RuntimeError('SWD parity mismatch')
                return ''
            d.call=call
            with self.assertRaisesRegex(RuntimeError,'SWD read failed at 0x4000'):
                d.dump(d.root/'result.bin',0x4000,8)
