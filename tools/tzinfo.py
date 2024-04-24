#!/usr/bin/python3

import os
import sys
import json

ZONEINFO_PATH = '/usr/share/zoneinfo'
TZDATA_PATH = ZONEINFO_PATH + '/tzdata.zi'

class TzData:
    def __init__(self):
        self.regions = {}
        self.comments = []

    def add_zone(self, zone: str):
        with open(os.path.join(ZONEINFO_PATH, zone), "rb") as f:
            data = f.read()
        if not data.startswith(b'TZif'):
            return
        data = data[:-1]
        nl = data.rfind(b'\n')
        if nl < 0:
            return
        info = data[nl+1:].decode("utf-8")
        region_name, _, zone_name = zone.rpartition('/')
        region_data = self.regions.setdefault(region_name, {})
        region_data[zone_name] = info

    def load(self):
        for line in open(TZDATA_PATH):
            line = line.strip()
            if line.startswith('# '):
                self.comments.append(line[2:])
                continue
            line, _, _ = line.partition('#')
            fields = line.split()
            if fields[0] == 'Z':
                self.add_zone(fields[1])

    def print(self, f, define: bool):
        print(file=f)
        for c in self.comments:
            print(f'// {c}', file=f)
        print(file=f)

        print('namespace TZ {', file=f)
        for region in sorted(self.regions):
            if region:
                region_tag = f'TZREGION_{region.replace('/', '_')}'
                print(f'namespace {region.replace('/', '::')} {{', file=f)
            else:
                region_tag = 'TZREGION_NONE'
            if define:
                print(f'  DEFINE_FSTR_LOCAL({region_tag}, "{region}")', file=f)
            for tag, value in self.regions[region].items():
                tag = tag.replace('-', '_N')
                tag = tag.replace('+', '_P')
                if define:
                    print(f'''
  DEFINE_FSTR_LOCAL(TZNAME_{tag}, "{tag}")
  DEFINE_FSTR_LOCAL(TZSTR_{tag}, "{value}")
  const TzInfo {tag} PROGMEM {{
      .region = {region_tag},
      .name = TZNAME_{tag},
      .rules = TZSTR_{tag},
  }};''', file=f)
                else:
                    print(f'  extern const TzInfo {tag};', file=f)
            if region:
                print(f'}} // namespace {region}', file=f)
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
