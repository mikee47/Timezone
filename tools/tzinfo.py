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

def get_day_number(name: str) -> int:
    name = match_name(DAY_NAMES, name)
    return DAY_NAMES.index(name)


def make_tag(path: str) -> str:
    return path.replace('/', '_')

def make_namespace(path: str) -> str:
    return path.replace('/', '::')


@dataclass
class On:
    expr: str

    # def __init__(self, value: str):
    #     if value.startswith('last'):
    #         self.expr = 'last' + match_day_name(value[4:])[:3]
    #     elif '0' <= value[0] <= '9':
    #         self.expr = value
    #     else:
    #         day = re.match('[a-zA-Z]+', value)[0]
    #         tail = value.removeprefix(day)
    #         day = match_day_name(day)[:3]
    #         self.expr = day + tail

    def __str__(self):
        return self.expr

    def get_day(self, year: int, month: str | int):
        value = self.expr
        if value.startswith('last'):
            # lastSun  the last Sunday in the month
            # lastMon  the last Monday in the month
            day = get_day_number(value[4:])
        elif '0' <= value[0] <= '9':
            # 5        the fifth of the month
            return int(value)
        else:
            # Sun>=8   first Sunday on or after the eighth
            # Sun<=25  last Sunday on or before the 25th
            # The “<=” and “>=” constructs can result in a day in the neighboring month; for example, the
            # IN-ON combination “Oct Sun>=31” stands for the first Sunday on or after October 31,
            # even if that Sunday occurs in November.
            day = re.match('[a-zA-Z]+', value)[0]
            tail = value.removeprefix(day)
            day = get_day_number(day)
            if tail.startswith('>='):
                assert(false)
            elif tail.startswith('<='):
                assert(false)
            else:
                assert(false)
            self.expr = day + tail

        if isinstance(month, str):
            month = get_month_number(month)



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
        fields = s.split(':')
        if fields:
            self.hour = int(fields.pop(0))
        if fields:
            self.min = int(fields.pop(0))
        if fields:
            self.sec = int(fields.pop(0))

    def __str__(self):
        return f'{self.hour:02d}:{self.min:02d}:{self.sec:02d}{self.timefmt}'

    @property
    def delta(self):
        return timedelta(hours=self.hour, minutes=self.min, seconds=self.sec)

@dataclass
class Until:
    year: int = None
    month: str = 'Jan'
    day: On = None
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
            self.day = On(fields.pop(0))
        else:
            self.day = On('1')
        if fields:
            # TIME (Rule AT)
            self.at = At(fields.pop(0))
            assert(not fields)
        else:
            self.at = At()

    def __bool__(self):
        return self.year is not None

    def __str__(self):
        return f'{self.year}/{self.month}/{self.day} {str(self.at)}' if self else ''
        # try:
        #     day = self.day.get_day(self.year, self.month)
        #     dt = datetime.fromisoformat(f'{self.year}-{get_month_number(self.month):02d}-{day:02d}')
        #     dt += self.at.delta
        #     return str(dt)
        # except:
        #     print(repr(self))
        #     return f'{self.year}/{self.month}/{self.day} {str(self.at)}'

    def applies_to(self, dt: datetime) -> bool:
        if dt is None or not self:
            return True
        if self.year < dt.year:
            return False
        if get_month_number(self.month) < dt.month:
            return False
        return True


@dataclass
class Rule:
    from_: int # YEAR
    to: int    # YEAR
    in_: str   # MONTH
    on: On     # DAY
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
        self.on = On(self.on)

    def __str__(self):
        return f'{self.from_} {self.to} - {self.in_} {self.on} {self.at} {self.save} {self.letters}'

    def applies_to(self, dt: datetime) -> bool:
        if dt is None:
            return True
        return self.from_ <= dt.year <= self.to


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
        s = f'{self.stdoff} {self.rule} {self.format}'
        if self.until:
            s += f' {self.until}'
        return s

    def applies_to(self, dt: datetime) -> bool:
        return self.until.applies_to(dt)


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
        self.rules: dict[list[Rule]] = {}
        self.zones: list[Zone | Link] = []

    def add_rule(self, fields: list[str]) -> Rule:
        name = fields[1]
        rule = Rule(fields[2], fields[3], fields[5], fields[6], fields[7], fields[8], fields[9])
        rules = self.rules.setdefault(name, [])
        rules.append(rule)
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
        now = datetime.now(timezone.utc)
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
                        if not zr.applies_to(now):
                            continue
                        f.write(f'      {zr}\n')
                        if zr.until.year:
                            print(zone, zr)

                        rules = self.rules.get(zr.rule)
                        if rules:
                            for r in rules:
                                if r.applies_to(now):
                                    f.write(f'        {r}\n')
                        elif zr.rule == '-':
                            pass
                        else:
                            f.write(f'        {zr.rule}\n')
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
