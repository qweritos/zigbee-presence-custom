import importlib.util
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch

SCRIPTS = Path(__file__).resolve().parents[1] / 'scripts'
sys.path.insert(0, str(SCRIPTS))
spec = importlib.util.spec_from_file_location('firmware_release', SCRIPTS / 'release.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ReleaseTests(unittest.TestCase):
    def test_tag_must_match_version_file(self):
        version, encoded, profiles = module.metadata()
        self.assertEqual(module.metadata('v' + version), (version, encoded, profiles))
        self.assertGreater(encoded, 9)
        self.assertTrue(profiles)
        with self.assertRaises(ValueError):
            module.metadata('v0.0.0')

    def test_collection_rejects_tampering_and_requires_all_models(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(
                module, 'metadata', return_value=('0.0.1', 256, {'zy-m100': {}})):
            root = Path(directory)
            (root / 'zy-m100.ota').write_bytes(b'validated-image')
            install_names = ['zy-m100.bin', 'zy-m100-bootloader.bin', 'zy-m100-loader.bin',
                             'zigbee_presence.py', 'LICENSE', 'NOTICE', 'upstream-license.txt',
                             'SiliconLabs-SDK-License.txt', 'README.md', 'hardware.md']
            for name in install_names:
                (root / name).write_bytes(b'installation-file')
            (root / 'debug.elf').write_bytes(b'debug-only')
            (root / 'intermediate.gbl').write_bytes(b'intermediate-only')
            artifacts = [{'file': name, 'size': (root / name).stat().st_size,
                          'sha256': hashlib.sha256((root / name).read_bytes()).hexdigest()}
                         for name in install_names + ['zy-m100.ota']]
            manifest = {'version': '0.0.1', 'ota_version': 256, 'artifacts': artifacts}
            (root / 'zy-m100-manifest.json').write_text(json.dumps(manifest))
            (root / 'zy-m100-index.json').write_text(json.dumps({'firmwares': [{'file_version': 256}]}))
            output = root / 'public'
            module.collect(root, output)
            self.assertEqual({p.name for p in output.iterdir()},
                             {'zy-m100-0.0.1.zip', 'zy-m100.ota', 'index.json', 'SHA256SUMS'})
            self.assertEqual(json.loads((output / 'index.json').read_text())['firmwares'][0]['file_version'], 256)
            with zipfile.ZipFile(output / 'zy-m100-0.0.1.zip') as z:
                self.assertEqual(set(z.namelist()), set(install_names + ['zy-m100-manifest.json']))
                bundled = json.loads(z.read('zy-m100-manifest.json'))
                for item in bundled['artifacts']:
                    self.assertEqual(hashlib.sha256(z.read(item['file'])).hexdigest(), item['sha256'])
            (root / 'zy-m100.ota').write_bytes(b'tampered-image!')
            with self.assertRaisesRegex(ValueError, 'checksum'):
                module.collect(root, output)
            manifest['ota_version'] = 9
            (root / 'zy-m100-manifest.json').write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, 'Mixed'):
                module.collect(root, output)
            with patch.object(module, 'metadata', return_value=('0.0.1', 256, {'missing-model': {}})):
                with self.assertRaises(FileNotFoundError):
                    module.collect(root, output)
