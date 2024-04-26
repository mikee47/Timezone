#!/usr/bin/python3
#
# Decodes IANA timezone database into python structures which can then be emitted in various
# formats as required by applications.
#

import os
import sys
import json
import re
from datetime import datetime, timezone, timedelta
from dataclasses import dataclass, field

# Where to find IANA timezone database locally
ZONEINFO_PATH = '/usr/share/zoneinfo'

# Format described here https://data.iana.org/time-zones/data/zic.8.txt
TZDATA_PATH = ZONEINFO_PATH + '/tzdata.zi'

MONTH_NAMES = [
    'January',
    'February',
    'March',
    'April',
    'May',
    'June',
    'July',
    'August',
    'September',
    'October',
    'November',
    'December',
]

DAY_NAMES = [
    'Sunday',
    'Monday',
    'Tuesday',
    'Wednesday',
    'Thursday',
    'Friday',
    'Saturday',
]

def match_name(values: list[str], name: str) -> str | None:
    for v in values:
        if v.startswith(name):
            return v
    return None

def match_month_name(name: str) -> str:
    month = match_name(MONTH_NAMES, name)
    if month:
        return month
    raise ValueError(f'Unknown month "{name}"')

def get_month_number(name: str) -> int:
    name = match_name(MONTH_NAMES, name)
    return 1 + MONTH_NAMES.index(name)

def match_day_name(name: str) -> str:
    day = match_name(DAY_NAMES, name)
    if day:
        return day
    raise ValueError(f'Unknown day "{name}"')


def make_tag(path: str) -> str:
    return path.replace('/', '_')

def make_namespace(path: str) -> str:
    return path.replace('/', '::')


@dataclass
class Rule:
    from_: int # YEAR
    to: int    # YEAR
    in_: str   # MONTH
    on: str    # DAY
    at: str    # TIME of day
    save: str
    letters: str

    def __post_init__(self):
        if self.from_.startswith('mi'):
            self.from_ = 0
        else:
            self.from_ = int(self.from_)
        if self.to.startswith('o'):
            self.to = self.from_
        elif self.to.startswith('ma'):
            self.to = 9999
        else:
            self.to = int(self.to)
        self.in_ = match_month_name(self.in_)[:3]
        self.on = self.parse_on(self.on)

    @staticmethod
    def parse_on(value: str) -> str:
        if value.startswith('last'):
            return 'last' + match_day_name(value[4:])[:3]
        if '0' <= value[0] <= '9':
            return value
        day = re.match('[a-zA-Z]+', value)[0]
        tail = value.removeprefix(day)
        day = match_day_name(day)[:3]
        return day + tail

    def __str__(self):
        return f'{self.from_} {self.to} - {self.in_} {self.on} {self.at} {self.save} {self.letters}'

    def applies_to_year(self, year: int) -> bool:
        return self.from_ <= year <= self.to


@dataclass
class At:
    hour: int = 0
    min: int = 0
    sec: int = 0
    timefmt: str = ''

    def __init__(self, s: str = None):
        if s is None:
            return
        self.timefmt = s[-1]
        if 'a' <= self.timefmt <= 'z':
            s = s[:-1]
        else:
            self.timefmt = ''
        s = s.split(':')
        self.hour = int(s.pop(0)) if s else 0
        self.min = int(s.pop(0)) if s else 0
        self.sec = int(s.pop(0)) if s else 0

    def __str__(self):
        return f'{self.hour:02d}:{self.min:02d}:{self.sec:02d}{self.timefmt}'

    @property
    def delta(self):
        return timedelta(hours=self.hour, minutes=self.min, seconds=self.sec)


@dataclass
class Until:
    year: int = None
    month: str = 'Jan'
    day: int = 1
    at: At = None

    def __init__(self, fields: list[str]):
        if fields:
            # YEAR
            self.year = int(fields.pop(0))
        if fields:
            # MONTH (Rule IN)
            self.month = match_month_name(fields.pop(0))[:3]
        if fields:
            # DAY (Rule ON)
            self.day = Rule.parse_on(fields.pop(0))
        if fields:
            # TIME (Rule AT)
            self.at = At(fields.pop(0))
            assert(not fields)
        else:
            self.at = At()

    def __str__(self):
        if self.year is None:
            return ''
        try:
            dt = datetime.fromisoformat(f'{self.year}-{get_month_number(self.month):02d}-{self.day:02d}')
            dt += self.at.delta
            return str(dt)
        except:
            return f'{self.year}/{self.month}/{self.day} {str(self.at)}'


@dataclass
class ZoneRule:
    stdoff: str
    rule: str
    format: str
    until: Until

    def __post_init__(self):
        at = At(self.stdoff)
        assert(not at.timefmt)
        self.stdoff = str(at)

    def __str__(self):
        return f'{self.stdoff} {self.rule} {self.format} {self.until}'



@dataclass
class NamedItem:
    name: str

    def __str__(self):
        return self.name

    def __lt__(self, other):
        return self.name < str(other)

    @property
    def region(self) -> str:
        return self.name.rpartition('/')[0]

    @property
    def zone_name(self) -> str:
        return self.name.rpartition('/')[2]

    @property
    def namespace(self) -> str:
        return make_namespace(self.region)

    @property
    def tag(self) -> str:
        return self.zone_name.replace('-', '_N').replace('+', '_P')


@dataclass
class Zone(NamedItem):
    tzstr: str
    rules: list[ZoneRule] = field(default_factory=list)

    def add_rule(self, fields: list[str]):
        stdoff = fields.pop(0)
        rule = fields.pop(0)
        format = fields.pop(0)
        until = Until(fields)
        rule = ZoneRule(stdoff, rule, format, until)
        self.rules.append(rule)
        return rule


@dataclass
class Link(NamedItem):
    zone: Zone


class TzData:
    def __init__(self):
        self.comments = []
        self.rules = {}
        self.zones = []

    def add_rule(self, fields: list[str]) -> Rule:
        name = fields[1]
        rule = self.rules[name] = Rule(fields[2], fields[3], fields[5], fields[6], fields[7], fields[8], fields[9])
        return rule

    def add_zone(self, fields: list[str]) -> Zone:
        name = fields[1]
        with open(os.path.join(ZONEINFO_PATH, name), "rb") as f:
            data = f.read()
        if not data.startswith(b'TZif'):
            return
        data = data[:-1]
        nl = data.rfind(b'\n')
        if nl < 0:
            return
        tzstr = data[nl+1:].decode("utf-8")
        zone = Zone(name, tzstr)
        zone.add_rule(fields[2:])
        self.zones.append(zone)
        return zone

    def add_link(self, fields: list[str]):
        tgt_name, link_name = fields[1:3]
        if link_name in self.zones:
            print(f'{link_name} already specified, skipping')
            return
        zone = next(z for z in self.zones if z.name == tgt_name)
        link = Link(link_name, zone)
        self.zones.append(link)

    def load(self):
        zone = None
        for line in open(TZDATA_PATH):
            line = line.strip()
            if line.startswith('# '):
                self.comments.append(line[2:])
                continue
            line, _, _ = line.partition('#')
            fields = line.split()
            rectype = fields[0]
            if rectype == 'Z':
                zone = self.add_zone(fields)
            elif rectype == 'R':
                zone = None
                self.add_rule(fields)
            elif rectype == 'L':
                zone = None
                self.add_link(fields)
            elif zone:
                zone.add_rule(fields)
        self.zones.sort()

    def write_file(self, f, define: bool):
        f.write('\n')
        for c in self.comments:
            f.write(f'// {c}\n')
        f.write('\n')

        f.write('namespace TZ {\n')
        for region in sorted(set(z.region for z in self.zones)):
            if region:
                region_tag = 'TZREGION_' + make_tag(region)
                f.write(f'namespace {make_namespace(region)} {{\n')
            else:
                region_tag = 'TZREGION_NONE'
            if define:
                f.write(f'  DEFINE_FSTR_LOCAL({region_tag}, "{region}")\n')
            for zone in [z for z in self.zones if z.region == region]:
                tag = zone.tag
                if define:
                    f.write(f'''
  /*
     {zone.name}
''')
                    if isinstance(zone, Link):
                        link = zone
                        zone = link.zone
                        f.write(f'''  */
  const TzInfo& {tag} PROGMEM = TZ::{zone.namespace}::{zone.tag};
''')
                        continue
                    for zr in zone.rules:
                        f.write(f'      {zr}\n')
                        r = self.rules.get(zr.rule)
                        if r:
                            f.write(f'        {r}\n')
                    f.write(f'''  */
  DEFINE_FSTR_LOCAL(TZNAME_{tag}, "{zone.zone_name}")
  DEFINE_FSTR_LOCAL(TZSTR_{tag}, "{zone.tzstr}")
  const TzInfo {tag} PROGMEM {{
      .region = {region_tag},
      .name = TZNAME_{tag},
      .rules = TZSTR_{tag},
  }};
''')
                else:
                    ref = '&' if isinstance(zone, Link) else ''
                    f.write(f'  extern const TzInfo{ref} {tag};\n')
            if region:
                f.write(f'}} // namespace {make_namespace(region)}\n')
            f.write('\n')
        f.write('} // namespace TZ\n')


def create_file(filename: str):
    f = open(filename, 'w')
    f.write(f'''\
/*
 * This file is auto-generated.
 */

// clang-format off
''')
    return f
    

def main():
    data = TzData()
    data.load()
    with create_file('tzdata.h') as f:
        f.write('''
#pragma once

#include <WString.h>

struct TzInfo {
    const FSTR::String& region;
    const FSTR::String& name;
    const FSTR::String& rules;

    String fullName() const
    {
        String s;
        if(region.length() != 0) {
            s += region;
            s += '/';
        }
        s += name;
        return s;
    }

};
''')
        data.write_file(f, False)
    with create_file('tzdata.cpp') as f:
        f.write('''
#include "tzdata.h"
''')
        data.write_file(f, True)


if __name__ == '__main__':
    main()
