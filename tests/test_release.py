import importlib.util
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
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
            item = {'file': 'zy-m100.ota', 'size': 15,
                    'sha256': hashlib.sha256(b'validated-image').hexdigest()}
            manifest = {'version': '0.0.1', 'ota_version': 256, 'artifacts': [item]}
            (root / 'zy-m100-manifest.json').write_text(json.dumps(manifest))
            (root / 'zy-m100-index.json').write_text(json.dumps({'firmwares': [{'file_version': 256}]}))
            module.collect(root)
            self.assertEqual(json.loads((root / 'index.json').read_text())['firmwares'][0]['file_version'], 256)
            (root / 'zy-m100.ota').write_bytes(b'tampered-image!')
            with self.assertRaisesRegex(ValueError, 'checksum'):
                module.collect(root)
            manifest['ota_version'] = 9
            (root / 'zy-m100-manifest.json').write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, 'Mixed'):
                module.collect(root)
            with patch.object(module, 'metadata', return_value=('0.0.1', 256, {'missing-model': {}})):
                with self.assertRaises(FileNotFoundError):
                    module.collect(root)
