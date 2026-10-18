import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('release_version', Path(__file__).resolve().parents[1] / 'scripts/version.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class VersionTests(unittest.TestCase):
    def test_release_is_newer_than_development_and_orders_semantically(self):
        encoded = lambda value: module.release_version(value)[1]
        self.assertEqual(encoded('0.0.1'), 256)
        self.assertGreater(encoded('0.0.1'), 9)
        self.assertGreater(encoded('0.1.0'), encoded('0.0.255'))
        self.assertGreater(encoded('1.0.0'), encoded('0.255.255'))
        self.assertEqual(encoded('255.255.255'), 0xffffff00)

    def test_rejects_invalid_or_ambiguous_versions(self):
        for text in ['0.0.0', 'v0.0.1', '01.0.1', '1.2', '1.2.3.4', '256.0.0',
                     '-1.0.1', '0.0.1-beta', '0.0.1\n']:
            with self.subTest(text=text), self.assertRaises(ValueError):
                module.release_version(text)
