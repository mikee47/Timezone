#!/usr/bin/python3

import os
import sys
import json

ZONEINFO_PATH = '/usr/share/zoneinfo'
TZDATA_PATH = ZONEINFO_PATH + '/tzdata.zi'
INDENT = '  '

class TzData(dict):
    def __init__(self):
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
        data = self
        for seg in zone.split('/'):
            data = data.setdefault(seg, {})
        data['TZSTR'] = info

    def load(self):
        for line in open(TZDATA_PATH):
            line = line.strip()
            if line.startswith('# '):
                self.comments.append(line[2:])
                continue
            line = line.split()
            if line[0] == 'Z':
                self.add_zone(line[1])

    def print(self, f, define: bool):
        print(file=f)
        for c in self.comments:
            print(f'// {c}', file=f)
        print(file=f)

        def print_zone(indent, tag, value):
            tag = tag.replace('-', '_N')
            tag = tag.replace('+', '_P')
            if isinstance(value, dict):
                print(f'{indent}namespace {tag} {{', file=f)
                for a, b in value.items():
                    print_zone(indent + INDENT, a, b)
                print(f'{indent}}} // namespace {tag}\n', file=f)
            elif define:
                print(f'{indent}DEFINE_FSTR({tag}, "{value}")', file=f)
            else:
                print(f'{indent}DECLARE_FSTR({tag})', file=f)
        print_zone('', 'TZ', self)


def create_file(filename: str):
    f = open(filename, 'w')
    print(f'''
/*
 * This file is auto-generated.
 */
''', file=f)
    return f
    

def main():
    data = TzData()
    data.load()
    with create_file('tzdata.h') as f:
        print('''
#pragma once

#include <FlashString/String.hpp>
''', file=f)
        data.print(f, False)
    with create_file('tzdata.cpp') as f:
        print('''
#include "tzdata.h"
''', file=f)
        data.print(f, True)


if __name__ == '__main__':
    main()
