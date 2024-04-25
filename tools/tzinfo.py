#!/usr/bin/python3

import os
import sys
import json
import re
from datetime import datetime, timezone, timedelta
from dataclasses import dataclass, field

ZONEINFO_PATH = '/usr/share/zoneinfo'
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
class ZoneRule:
    stdoff: str
    rule: str
    format: str
    until: list | datetime

    def __post_init__(self):
        hr, min, sec, timefmt = self.decode_at(self.stdoff)
        assert(not timefmt)
        self.stdoff = f'{hr:02d}:{min:02d}:{sec:02d}'

        fields = self.until
        if not fields:
            self.until = None
            return
        # YEAR
        year = int(fields.pop(0))
        if fields:
            # MONTH (Rule IN)
            month = match_month_name(fields.pop(0))[:3]
        else:
            month = 'Jan'
        if fields:
            # DAY (Rule ON)
            day = Rule.parse_on(fields.pop(0))
        else:
            day = 1
        if fields:
            # TIME (Rule AT)
            hr, min, sec, timefmt = self.decode_at(fields.pop(0))
            assert(not fields)
            time = f'{hr:02d}:{min:02d}:{sec:02d}'
        else:
            hr, min, sec, timefmt = 0, 0, 0, ''
            time = '00:00:00'
        try:
            dt = datetime.fromisoformat(f'{year}-{get_month_number(month):02d}-{int(day):02d}')
            dt += timedelta(hours=hr, minutes=min, seconds=sec)
            self.until = dt
        except:
            self.until = [year, month, day, time, timefmt]
            print(self.until)

    @staticmethod
    def decode_at(s: str) -> tuple[int, int, int, str]:
        timefmt = s[-1]
        if 'a' <= timefmt <= 'z':
            s = s[:-1]
        else:
            timefmt = ''
        s = s.split(':')
        hr = int(s.pop(0)) if s else 0
        min = int(s.pop(0)) if s else 0
        sec = int(s.pop(0)) if s else 0
        return (hr, min, sec, timefmt)


    def __str__(self):
        return f'{self.stdoff} {self.rule} {self.format} {self.until}'


@dataclass
class Zone:
    tzstr: str
    rules: list[ZoneRule] = field(default_factory=list)

    def add_rule(self, fields: list[str]):
        stdoff = fields.pop(0)
        rule = fields.pop(0)
        format = fields.pop(0)
        until = fields
        rule = ZoneRule(stdoff, rule, format, until)
        self.rules.append(rule)
        return rule

@dataclass
class Link:
    region_name: str
    zone_name: str
    zone: Zone

Region = dict[Zone | Link]

class TzData:
    def __init__(self):
        self.comments = []
        self.rules = {}
        self.regions: dict[Region] = {}

    def add_rule(self, fields: list[str]) -> Rule:
        name = fields[1]
        rule = self.rules[name] = Rule(fields[2], fields[3], fields[5], fields[6], fields[7], fields[8], fields[9])
        return rule

    @staticmethod
    def split_zone_path(path: str) -> tuple[str, str]:
       x = path.rpartition('/')
       return (x[0], x[2])
 
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
        region_name, zone_name = self.split_zone_path(name)
        region = self.regions.setdefault(region_name, Region())
        zone = region[zone_name] = Zone(tzstr)
        zone.add_rule(fields[2:])
        return zone

    def add_link(self, fields: list[str]):
        tgt_region_name, tgt_zone_name = self.split_zone_path(fields[1])
        region_name, zone_name = self.split_zone_path(fields[2])
        tgt_zone = self.regions[tgt_region_name][tgt_zone_name]
        region = self.regions.setdefault(region_name, Region())
        if zone_name in region:
            print(f'{zone_name} already in region {region_name}')
            print(fields)
        else:
            link = region[zone_name] = Link(tgt_region_name, tgt_zone_name, tgt_zone)

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

    @staticmethod
    def get_zone_tag(name: str) -> str:
        return name.replace('-', '_N').replace('+', '_P')

    @staticmethod
    def get_full_name(region_name: str, zone_name: str) -> str:
        if region_name:
            return region_name + '/' + zone_name
        return zone_name
        
    def write_file(self, f, define: bool):
        f.write('\n')
        for c in self.comments:
            f.write(f'// {c}\n')
        f.write('\n')

        f.write('namespace TZ {\n')
        for region_name in sorted(self.regions):
            if region_name:
                region_tag = f'TZREGION_{region_name.replace('/', '_')}'
                region_ns = region_name.replace('/', '::')
                f.write(f'namespace {region_ns} {{\n')
            else:
                region_tag = 'TZREGION_NONE'
            if define:
                f.write(f'  DEFINE_FSTR_LOCAL({region_tag}, "{region_name}")\n')
            region = self.regions[region_name]
            for zone_name in sorted(region):
                zone = region[zone_name]
                tag = self.get_zone_tag(zone_name)
                if isinstance(zone, Link):
                    link = zone
                    link_tag = self.get_zone_tag(link.zone_name)
                else:
                    link = None
                if define:
                    f.write(f'''
  /*
     {self.get_full_name(region_name, zone_name)}
''')
                    if link:
                        tgt_region_ns = link.region_name.replace('/', '::')
                        f.write(f'''  */
  const TzInfo& {tag} PROGMEM = TZ::{tgt_region_ns}::{link_tag};
''')
                        continue
                    for zr in zone.rules:
                        f.write(f'      {zr}\n')
                        r = self.rules.get(zr.rule)
                        if r:
                            f.write(f'        {r}\n')
                    f.write(f'''  */
  DEFINE_FSTR_LOCAL(TZNAME_{tag}, "{zone_name}")
  DEFINE_FSTR_LOCAL(TZSTR_{tag}, "{zone.tzstr}")
  const TzInfo {tag} PROGMEM {{
      .region = {region_tag},
      .name = TZNAME_{tag},
      .rules = TZSTR_{tag},
  }};
''')
                elif link:
                    f.write(f'  extern const TzInfo& {tag};\n')
                else:
                    f.write(f'  extern const TzInfo {tag};\n')
            if region_name:
                f.write(f'}} // namespace {region_ns}\n')
            f.write('\n')
        f.write('} // namespace TZ\n')

        # print("\n".join(f'{k} = {v}' for k, v in self.links.items()))

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
