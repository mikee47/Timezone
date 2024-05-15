"""
IANA timezone database utilities
"""

import os
import re
from importlib import resources

# File containing compact textual source of IANA database, with version
TZDATA_ZI = 'tzdata.zi'

# The default cached path for zone information
ZONEINFO_PATH = None

ZONE_AREAS = [
    'Africa',
    'America',
    'Antarctica',
    'Arctic',
    'Asia',
    'Atlantic',
    'Australia',
    'Europe',
    'Indian',
    'Pacific',
]


# Normalise path on Windows, backslashes are problematic
def normalise_path(path: str):
    print(f'normalise {path}')
    return path.replace('\\', '/')


def find_zoneinfo_path():
    global ZONEINFO_PATH
    if ZONEINFO_PATH:
        return
    # Search list of standard directories
    try: # Requires python 3.9+
        import zoneinfo
        for path in zoneinfo.TZPATH:
            if os.path.exists(os.path.join(path, 'GMT')):
                ZONEINFO_PATH = normalise_path(path)
                return
    except ImportError:
        pass
    # Last resort, use tzdata package (if installed)
    try:
        ZONEINFO_PATH = normalise_path(os.path.join(resources.files('tzdata'), 'zoneinfo'))
    except AttributeError:
        # Python < 3.9
        with resources.path('tzdata', 'zoneinfo') as path:
            ZONEINFO_PATH = str(path)
        


def get_zoneinfo_path() -> str:
    if not ZONEINFO_PATH:
        find_zoneinfo_path()
    return ZONEINFO_PATH


def get_zoneinfo_version(zoneinfo_path: str = None) -> str:
    if not zoneinfo_path:
        zoneinfo_path = get_zoneinfo_path()
    names = ['version', TZDATA_ZI]
    for name in names:
        filename = os.path.join(zoneinfo_path, name)
        if not os.path.exists(filename):
            continue
        with open(filename) as f:
            # First line looks like '# version 2024a'
            return f.readline().strip().rpartition(' ')[2]
    if not os.path.exists(filename):
        return '0000x'


class ZoneList(list):
    def __init__(self, zoneinfo_path: str = None):
        if not zoneinfo_path:
            zoneinfo_path = get_zoneinfo_path()
        self.version = get_zoneinfo_version(zoneinfo_path)
        zones = set()
        def scan_dirs(root, dirs):
            for d in dirs:
                for wroot, wdir, wnames in os.walk(os.path.join(zoneinfo_path, root, d)):
                    for name in wnames:
                        if '.' in name: # tzdata package has non-TZif files here
                            continue
                        path = os.path.join(wroot, name)
                        zone = os.path.relpath(path, zoneinfo_path).replace('\\', '/')
                        zones.add(zone)
                    scan_dirs(wroot, wdir)
        scan_dirs('', ZONE_AREAS)
        self.extend(sorted(zones))

    def find_matches(self, name: str):
        def get_cmpstr(s: str):
            return re.sub(r'[^a-zA-Z]', '', s.lower())
        matches = []
        cmp_name = get_cmpstr(name)
        for z in self:
            cmpz = get_cmpstr(z)
            if cmpz == cmp_name:
                return [z] # Exact match
            if cmpz.startswith(cmp_name):
                matches.append(z) # Partial match
        return matches
