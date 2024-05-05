#!/usr/bin/python3
#
# Scan timezone TZif files
#
#

import os
import sys
from struct import unpack, calcsize
from tzinfo import ZONEINFO_PATH, TzString
from dataclasses import dataclass
from datetime import datetime, UTC, timedelta


def readfmt(fp, fmt: str):
    size = calcsize(fmt)
    data = fp.read(size)
    return unpack(fmt, data)


@dataclass 
class Header:
    magic: str
    fmtver: int
    ttisutccnt: int
    ttisstdcnt: int
    leapcnt: int
    timecnt: int
    typecnt: int
    charcnt: int

    def __post_init__(self):
        # Make sure it is a tzfile(5) file
        assert self.magic == b'TZif', f'Got magic {self.magic}'


@dataclass
class TTInfo:
    owner: object # TzInfo
    tt_utoff: int
    tt_isdst: bool
    tt_desigidx: int

    @property
    def tzname(self):
        return self.owner.get_tzname(self.tt_desigidx)

    def __repr__(self):
        return f'{self.tt_utoff} {self.tt_isdst} {self.tzname}'


@dataclass
class Leap:
    utc: int
    correction: int


@dataclass
class TzInfo:
    hdr: Header
    info: list[TTInfo]
    transitions: list[int]
    lindexes: bytearray
    ttinfo_raw: list[TTInfo]
    tznames_raw: list[str]
    leap_raw: list[Leap]
    stdwall_raw: list[bool]
    utlocal_raw: list[bool]

    def __init__(self, fp, time64: bool):
        timefmt = "q" if time64 else "l"
        hdr = self.hdr = Header(*readfmt(fp, '>4s c 15x 6L'))
        self.transitions = readfmt(fp, f'>{hdr.timecnt}{timefmt}')
        self.lindexes = list(readfmt(fp, f'>{hdr.timecnt}B'))
        self.ttinfo_raw = [TTInfo(self, *readfmt(fp, f'>lBB')) for _ in range(hdr.typecnt)]
        self.tznames_raw, = readfmt(fp, f'>{hdr.charcnt}s')
        self.leap_raw = [readfmt(fp, f'>{timefmt}{timefmt}') for _ in range(hdr.leapcnt)]
        self.stdwall_raw = readfmt(fp, f'>{hdr.ttisstdcnt}B')
        self.utlocal_raw = readfmt(fp, f'>{hdr.ttisutccnt}B')

    def get_ttinfo(self, i: int) -> TTInfo:
        idx = self.lindexes[i]
        return self.ttinfo_raw[idx]

    def get_tzname(self, tt_desigidx: int):
        s = ''
        for c in self.tznames_raw[tt_desigidx:]:
            if c == 0:
                break
            s += chr(c)
        return s

    @classmethod
    def __repr__(cls):
        return cls.__name__


@dataclass
class TzFile:
    info: list[TzInfo]
    tzstr: str

    def __init__(self, fp):
        info = TzInfo(fp, False)
        self.info = [info]
        if info.hdr.fmtver >= b'2':
            info = TzInfo(fp, True)
            self.info.append(info)
            self.tzstr = fp.read().strip().decode()
        else:
            self.tzstr = None


def scan_zonefile(filename: str):
    print(f'Scanning {filename}')
    with open(filename, "rb") as f:
        tzfile = TzFile(f)
        filesize = f.tell()

    time_from = datetime(2024, 1, 1).timestamp()
    time_to = datetime(2034, 1, 1).timestamp()

    # print(name, tzfile.tzstr)
    info = tzfile.info[-1]
    # print(' ', info.hdr)
    def print_transition(it: int):
        time = info.transitions[it]
        ttinfo = info.get_ttinfo(it)
        dt = datetime.fromtimestamp(time, tz=UTC)
        dt_new = dt + timedelta(seconds=ttinfo.tt_utoff)
        if it > 0:
            ttinfo_prev = info.get_ttinfo(it - 1)
            dt_old = dt + timedelta(seconds=ttinfo_prev.tt_utoff)
            print(f'    {dt.ctime()} UTC, {dt_old.ctime()} {ttinfo_prev.tzname} -> {dt_new.ctime()} {ttinfo.tzname}, {ttinfo}')
        else:
            print(f'    {dt.ctime()} UTC -> {dt_new.ctime()} {ttinfo.tzname}, {ttinfo}')

    timecnt = 0
    last_transition = None
    for i in range(info.hdr.timecnt):
        time = info.transitions[i]
        if time >= time_to:
            break
        if time >= time_from:
            print_transition(i)
            timecnt += 1
        last_transition = i
    if timecnt == 0:
        print_transition(last_transition)


def scan_zone(name: str):
    scan_zonefile(os.path.join(ZONEINFO_PATH, name))


continents = [
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

def scan_dirs(root, dirs):
    for d in dirs:
        for wroot, wdir, wnames in os.walk(os.path.join(ZONEINFO_PATH, root, d)):
            for name in wnames:
                scan_zonefile(os.path.join(wroot, name))
            scan_dirs(os.path.join(wroot), wdir)
            # print('ROOT', wroot)
            # print('DIR', wdir)
            # print('NAMES', wnames)

def scan_all():
    scan_dirs('', continents)

def main():
    scan_all()
    return

    # scan_zone('Australia/Melbourne')
    scan_zone('Europe/London')
    scan_zone('Asia/Gaza')
    scan_zone('America/Nuuk')
    scan_zone('America/Scoresbysund')

if __name__ == '__main__':
    main()
