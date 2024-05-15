"""
Scan POSIX timezone strings.

https://sourceware.org/glibc/manual/2.39/html_node/TZ-Variable.html

Only the 'M' format is implemented as it's the only one currently in use
"""

import os
import re
from dataclasses import dataclass

DST_OFFSET_DEFAULT = 3600


@dataclass
class Time:
    hour: int = 2
    minute: int = 0
    second: int = 0

    def __init__(self, s: str = None):
        if s is None:
            return
        fields = s.split(':')
        if fields:
            self.hour = int(fields.pop(0))
            # Note: POSIX says non-negative, but Scoresbysund and Nuuk both violate this
        if fields:
            self.minute = int(fields.pop(0))
        if fields:
            self.second = int(fields.pop(0))


def decode_offset(s: str) -> int:
    fields = s.split(':')
    s = fields.pop(0)
    if s[0] == '-':
        sign = -1
        s = s[1:]
    else:
        sign = 1
    hours = int(s);
    mins = int(fields.pop(0)) if fields else 0
    secs = int(fields.pop(0)) if fields else 0
    seconds = (hours * 3600) + (mins * 60) + secs
    # Offset is 'West of UT' so negate it
    return -seconds * sign


@dataclass
class Rule:
    name: str
    offset: int
    month: int = 0
    week: int = 1   # 1 <= week <= 5 (5 indicates 'last')
    day: int = 0    # 0=Sunday
    time: Time = None

    def __init__(self, name: str, offset: int, expr: str):
        self.name = name.strip('<>')
        self.offset = offset
        if expr is None:
            self.time = Time()
            return
        m = re.match(r'M(\d+)\.(\d+)\.(\d+)/?(.+)?', expr)
        g = m.groups()
        self.month = int(g[0]) - 1
        self.week = int(g[1])
        self.day = int(g[2])
        self.time = Time(g[3])


@dataclass
class RulePair:
    std: Rule
    dst: Rule


def decode_tzstr(tzstr: str) -> RulePair:
    try:
        if tzstr[0] == ':':
            tzstr = tzstr[1:]
        m = re.match(r'''
            (<[^>]+>|[a-zA-Z]+)([\d:\-+]+)      # std offset
            (<[^>]+>|[a-zA-Z]+)?([\d:\-+]+)?    # [ dst [offset]
            ,?([^,]+)?,?([^,]+)?                # [ , date [ / time ] [,date[/time]] ]
            ''', tzstr, flags=re.VERBOSE)
        g = m.groups()
        std = Rule(g[0], decode_offset(g[1]), g[5])
        if g[2]:
            if g[3]:
                offset = decode_offset(g[3])
            else:
                offset = std.offset + DST_OFFSET_DEFAULT
            dst = Rule(g[2], offset, g[4])
        else:
            dst = None
        return RulePair(std, dst)
    except:
        raise ValueError(f'Invalid TZ string "{tzstr}"')
