#!/usr/bin/python3

import os
import sys
import json
from dataclasses import dataclass, field

ZONEINFO_PATH = '/usr/share/zoneinfo'
TZDATA_PATH = ZONEINFO_PATH + '/tzdata.zi'

@dataclass
class Rule:
    from_: str
    to: str
    in_: str
    on: str
    at: str
    save: str
    letters: str

    # def __post_init__(self):
    #     if self.from_.startswith('mi'):
    #         self.from_ = 0
    #     if self.to.startswith('o'):
    #         self.to = self.from_

    def __str__(self):
        return f'{self.from_} {self.to} - {self.in_} {self.on} {self.at} {self.save} {self.letters}'

@dataclass
class ZoneRule:
    stdoff: str
    rule: str
    format: str
    until: str

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
        until = fields.pop(0) if fields else None
        rule = ZoneRule(stdoff, rule, format, until)
        self.rules.append(rule)
        return rule


Region = dict[Zone]

class TzData:
    def __init__(self):
        self.rules = {}
        self.regions = {}
        self.comments = []

    def add_rule(self, fields: list[str]) -> Rule:
        name = fields[1]
        rule = self.rules[name] = Rule(fields[2], fields[3], fields[5], fields[6], fields[7], fields[8], fields[9])
        # yr_from = int(rule.from_)
        # yr_to = int(rule.to)
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
        region_name, _, zone_name = name.rpartition('/')
        region = self.regions.setdefault(region_name, Region())
        zone = region[zone_name] = Zone(tzstr)
        zone.add_rule(fields[2:])
        return zone

    def add_link(self, fields: list[str]):
        pass

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
            for zone_name, zone in region.items():
                tag = zone_name.replace('-', '_N').replace('+', '_P')
                if define:
                    f.write(f'''
  /*
     {'/'.join([region_name, zone_name])}
''')
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
                else:
                    f.write(f'  extern const TzInfo {tag};\n')
            if region_name:
                f.write(f'}} // namespace {region_ns}\n')
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
