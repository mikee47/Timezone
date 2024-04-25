#!/usr/bin/python3

import os
import sys
import json
from dataclasses import dataclass

ZONEINFO_PATH = '/usr/share/zoneinfo'
TZDATA_PATH = ZONEINFO_PATH + '/tzdata.zi'

@dataclass
class Rule:
    name: str

@dataclass
class Zone:
    tzstr: str

    def update(self, fields: list[str]):
        pass

Region = dict[Zone]

class TzData:
    def __init__(self):
        self.rules = {}
        self.regions = {}
        self.comments = []

    def add_rule(self, fields: list[str]) -> Rule:
        return Rule(fields[1])

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
                zone.update(fields)

    def print(self, f, define: bool):
        print(file=f)
        for c in self.comments:
            print(f'// {c}', file=f)
        print(file=f)

        print('namespace TZ {', file=f)
        for region_name in sorted(self.regions):
            if region_name:
                region_tag = f'TZREGION_{region_name.replace('/', '_')}'
                region_ns = region_name.replace('/', '::')
                print(f'namespace {region_ns} {{', file=f)
            else:
                region_tag = 'TZREGION_NONE'
            if define:
                print(f'  DEFINE_FSTR_LOCAL({region_tag}, "{region_name}")', file=f)
            region = self.regions[region_name]
            for zone_name, zone in region.items():
                tag = zone_name.replace('-', '_N').replace('+', '_P')
                if define:
                    print(f'''
  DEFINE_FSTR_LOCAL(TZNAME_{tag}, "{zone_name}")
  DEFINE_FSTR_LOCAL(TZSTR_{tag}, "{zone.tzstr}")
  const TzInfo {tag} PROGMEM {{
      .region = {region_tag},
      .name = TZNAME_{tag},
      .rules = TZSTR_{tag},
  }};''', file=f)
                else:
                    print(f'  extern const TzInfo {tag};', file=f)
            if region_name:
                print(f'}} // namespace {region_ns}', file=f)
            print(file=f)
        print('} // namespace TZ', file=f)


def create_file(filename: str):
    f = open(filename, 'w')
    print(f'''\
/*
 * This file is auto-generated.
 */

// clang-format off''', file=f)
    return f
    

def main():
    data = TzData()
    data.load()
    with create_file('tzdata.h') as f:
        print('''
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
''', file=f)
        data.print(f, False)
    with create_file('tzdata.cpp') as f:
        print('''
#include "tzdata.h"
''', file=f)
        data.print(f, True)


if __name__ == '__main__':
    main()
