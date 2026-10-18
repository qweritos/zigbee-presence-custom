"""Release versions use three 8-bit fields and reserve the low byte for builds."""
import re


def release_version(text):
    if not re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', text):
        raise ValueError('Expected a release version MAJOR.MINOR.PATCH')
    parts = tuple(map(int, text.split('.')))
    if any(p > 255 for p in parts) or parts == (0, 0, 0):
        raise ValueError('Version fields must fit uint8 and version must be nonzero')
    return parts, (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8)
