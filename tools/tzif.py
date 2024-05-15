"""
Scan timezone TZif files

See https://data.iana.org/time-zones/tzdb/tzfile.5.txt or `man tzfile`

TZif files are compiled from the IANA database sources (using `zic`) and provided with
all distributions of GNU/Linux, MacOS and FreeBSD as standard.

Each file corresponds to a specific IANA timezone (e.g. 'Europe/London') and contains
list of daylight savings transition times plus a POSIX rule string (see tzstr.py).
The purpose of the POSIX string is to allow transitions beyond those listed to be calculated.

Generally for future time transitions the POSIX string is sufficient.
Transition times are useful for testing though.

"""

from __future__ import annotations
import os
import re
from struct import unpack, calcsize
from dataclasses import dataclass


def readfmt(fp, fmt: str):
    size = calcsize(fmt)
    data = fp.read(size)
    return unpack(fmt, data)


@dataclass 
class Header:
    magic: str
    fmtver: int
    isutccnt: int
    isstdcnt: int
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

    @property
    def index(self):
        return self.owner.timetypes.index(self)

    @property
    def stdwall(self):
        return self.owner.stdwall[self.index] if self.owner.stdwall else 0

    @property
    def utlocal(self):
        return self.owner.utlocal[self.index] if self.owner.utlocal else 0

    @property
    def indicator(self):
        return 'z' if self.utlocal else 's' if self.stdwall else 'w'

    def __repr__(self):
        return f'{self.tt_utoff} {self.tt_isdst} {self.tzname} {self.indicator}'


@dataclass
class Leap:
    utc: int
    correction: int


@dataclass
class TzInfo:
    hdr: Header
    info: list[TTInfo]
    transitions: list[int]
    ttindex: bytearray
    timetypes: list[TTInfo]
    tznames: bytes
    leap: list[Leap]
    stdwall: list[bool]
    utlocal: list[bool]

    def __init__(self, fp, time64: bool):
        timefmt = "q" if time64 else "l"
        hdr = self.hdr = Header(*readfmt(fp, '>4s c 15x 6L'))
        self.transitions = readfmt(fp, f'>{hdr.timecnt}{timefmt}')
        self.ttindex = readfmt(fp, f'>{hdr.timecnt}B')
        self.timetypes = [TTInfo(self, *readfmt(fp, f'>lBB')) for _ in range(hdr.typecnt)]
        self.tznames, = readfmt(fp, f'>{hdr.charcnt}s')
        self.leap = [readfmt(fp, f'>{timefmt}l') for _ in range(hdr.leapcnt)]
        self.stdwall = readfmt(fp, f'>{hdr.isstdcnt}B')
        self.utlocal = readfmt(fp, f'>{hdr.isutccnt}B')

    def get_ttinfo(self, i: int) -> TTInfo:
        idx = self.ttindex[i]
        return self.timetypes[idx]

    def get_tzname(self, tt_desigidx: int):
        end = self.tznames.index(0, tt_desigidx)
        return self.tznames[tt_desigidx:end].decode()

    @classmethod
    def __repr__(cls):
        return cls.__name__


@dataclass
class TzFile:
    info: list[TzInfo]
    tzstr: str

    def __init__(self, filename: str):
        with open(filename, "rb") as fp:
            try:
                info = TzInfo(fp, False)
                self.info = [info]
                if info.hdr.fmtver >= b'2':
                    info = TzInfo(fp, True)
                    self.info.append(info)
                    self.tzstr = fp.read().strip().decode()
                else:
                    self.tzstr = None
            except Exception as e:
                raise RuntimeError(f'{e} reading {filename}')
