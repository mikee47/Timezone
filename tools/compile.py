"""
Parse the locally installed IANA database and generate C++ source code.
"""

from __future__ import annotations
import os
import io
import sys
import struct
from tzdb import ZONE_AREAS, ZoneList, get_zoneinfo_path, get_zoneinfo_version
from tzif import TzFile, TzInfo
from tzstr import Rule, RulePair, decode_tzstr
from dataclasses import dataclass
from datetime import datetime, timezone
from argparse import ArgumentParser

global args

WEEK_NAMES = [ 'First', 'Second', 'Third', 'Fourth', 'Last' ]
MONTH_NAMES = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec']
DAY_NAMES = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat']

def get_timestr(time: int):
    DATETIMEFMT = '%Y %a %b %d %H:%M:%S'
    dt = datetime.fromtimestamp(time, tz=timezone.utc)
    return dt.strftime(DATETIMEFMT)


def get_rule_def(rule: Rule) -> str:
    if rule.time.minute == 0:
        timestr = str(rule.time.hour)
    else:
        # -167 <= hour <= 167
        minutes = rule.time.hour * 60
        if minutes < 0:
            minutes -= rule.time.minute
        else:
            minutes += rule.time.minute
        timestr = str(minutes / 60)
    values = [
        f'{{"{rule.name}"}}',
        WEEK_NAMES[rule.week - 1],
        DAY_NAMES[rule.day],
        MONTH_NAMES[rule.month],
        timestr,
        str(rule.offset // 60),
    ]
    return ", ".join(values)


@dataclass
class Transition:
    time: int
    desigidx: int
    offset: int
    isdst: bool

    def pack(self) -> bytes:
        packed_time = struct.pack('<q', self.time)
        return struct.pack('<hb', (self.offset << 1) | self.isdst, self.desigidx) + packed_time[:5]

    @staticmethod
    def unpack(data: bytes) -> Transition:
        offset, desigidx = struct.unpack('<hb', data[:3])
        # Sign extend
        packed_time = data[3:] + (b'\xff\xff' if data[7] & 0x80 else b'\x00\x00')
        time, = struct.unpack('<q', packed_time)
        return Transition(time, desigidx, offset >> 1, bool(offset & 1))


def get_transitions(tzfile: TzFile) -> list[Transition]:
    from_ = getattr(args, 'from')
    time_from = int(datetime(from_, 1, 1, tzinfo=timezone.utc).timestamp())
    time_to = int(datetime(args.to, 12, 31, tzinfo=timezone.utc).timestamp())

    info = tzfile.info[-1]

    # Lists often have a final duplicate transition, sometimes (not always) at time uint32_max
    # Discard it until such time as we find a use for it
    num_transitions = len(info.transitions)
    if num_transitions >= 2 and info.get_ttinfo(num_transitions-2) == info.get_ttinfo(num_transitions-1):
        num_transitions -= 1

    tti_prev = None
    res = []
    for i, tt in enumerate(info.transitions[:num_transitions]):
        tti = info.get_ttinfo(i)

        # dt_raw = get_timestr(tt)
        # if tti_prev:
        #     dt_prev = f'{get_timestr(tt + tti_prev.tt_utoff)} {tti_prev.tzname}'
        # else:
        #     dt_prev = ''
        # dt = f'{get_timestr(tt + tti.tt_utoff)} {tti.tzname}'
        # print(f'{tt}\t{tti.tt_utoff:4}\t{tti.tt_isdst}\t{tti.indicator}\t{dt_raw}\t{dt_prev}\t{dt}')

        tti_prev = tti

        if tt > time_to:
            break
        if tt < time_from:
            res = []
        res.append(Transition(tt, tti.tt_desigidx, tti.tt_utoff // 60, bool(tti.tt_isdst)))
    return res


def get_namespace(s: str):
    return s.translate(str.maketrans({'/': '::', '-': '_'}))


def get_class_namespace(s: str) -> tuple[str, str]:
    ns = get_namespace(s)
    ns, _, clsname = ns.rpartition('::')
    return ns, clsname


@dataclass
class TimezoneInfo:
    name: str
    tzstr: str
    rules: RulePair
    info: TzInfo
    transitions: list[Transition] = None
    tzstr_alias: str = None
    transitions_alias: str = None 

    @property
    def area(self):
        return self.name.partition('/')[0]

    @property
    def location(self):
        return self.name.partition('/')[2]


def get_info(zone: str) -> TimezoneInfo:
    tzfile = TzFile(os.path.join(get_zoneinfo_path(), zone))
    try:
        rules = decode_tzstr(tzfile.tzstr)
    except:
        raise ValueError(f'Invalid TZ string "{tzfile.tzstr}"')
    transitions = get_transitions(tzfile) if rules.dst else None
    return TimezoneInfo(zone, tzfile.tzstr, rules, tzfile.info[-1], transitions)


def write_zone(fp, zone: TimezoneInfo):
    c = f' (same as {zone.tzstr_alias})' if zone.tzstr_alias else ''

    ns, clsname = get_class_namespace(zone.location)
    # ns_begin = f'namespace {ns} {{\n' if ns else ''
    # ns_end = '}\n' if ns else ''
    ns_begin = ns_end = ''

    lines = []

    if zone.tzstr_alias:
        lines.append(f'DEFINE_REF_LOCAL(tzstr, {get_namespace(zone.tzstr_alias)}::tzstr)')
    else:
        lines.append(f'TZ_DEFINE_PSTR_LOCAL(tzstr, "{zone.tzstr}")')

    lines.append(f'TZ_DEFINE_RULE_LOCAL(std_rule, {get_rule_def(zone.rules.std)})')
    if zone.rules.dst:
        lines.append(f'TZ_DEFINE_RULE_LOCAL(dst_rule, {get_rule_def(zone.rules.dst)})')
    else:
        lines.append('DEFINE_REF_LOCAL(dst_rule, rule_none)')

    s = zone.info.tznames.decode()[:-1]
    s = s.replace('\0', '\\0')
    lines.append(f'TZ_DEFINE_PSTR_LOCAL(tznames, "{s}")')

    if args.transitions and zone.transitions and not zone.transitions_alias:
        lines.append('DEFINE_FSTR_ARRAY_LOCAL(transitions, Transition,')
        for transition in zone.transitions:
            lines.append(f'\t{{{transition.time}, {transition.desigidx}, {transition.offset}, {int(transition.isdst)}}},\t// {get_timestr(transition.time)} UTC')
        lines.append(')')
    else:
        alias = f'{get_namespace(zone.transitions_alias)}::transitions' if zone.transitions_alias else 'transitions_none'
        lines.append(f'DEFINE_REF_LOCAL(transitions, {alias})')

    fp.write(f'\n{ns_begin}TIMEZONE_BEGIN({clsname}, "{zone.area}", "{zone.location}")\n')
    for line in lines:
        fp.write(f'\t{line}\n')
    fp.write(f'TIMEZONE_END()\n{ns_end}')


def write_zones_full():
    header = open(os.path.join(args.output, 'tzdata.h'), 'w')
    source = open(os.path.join(args.output, 'tzdata.cpp'), 'w')

    zoneinfo = [get_info(name) for name in ZoneList()]
    write_zones(zoneinfo, header, source)


def write_zones(zoneinfo: list[TimezoneInfo], header, source):
    ver = get_zoneinfo_version()
    ver_major = int(ver[:4])
    ver_minor = 1 + ord(ver[4]) - ord('a')

    intro = f'''\
/*
 *
 * IANA timezone database. Auto-generated file.
 *
 * source:  {get_zoneinfo_path()}
 * version: {ver}
 *
 * {len(zoneinfo)} zones.
 */
'''

    header.write(intro + f'''
#define TZINFO_WANT_NAME  {int(args.name)}
#define TZINFO_WANT_TZSTR {int(args.tzstr)}
#define TZINFO_WANT_RULES {int(args.rule)}
#define TZINFO_WANT_TRANSITIONS {int(args.transitions)}

#include <tzdb.h>

#define ZONEINFO_SOURCE "{get_zoneinfo_path()}"
// Original string version identifier
#define ZONEINFO_VERSION "{ver}"
// Semantic versioning
#define ZONEINFO_VER {hex(ver_major << 8 | ver_minor)}U
#define ZONEINFO_VER_MAJOR {ver_major}
#define ZONEINFO_VER_MINOR {ver_minor}

namespace TZ {{
''')

    # Build list of namespaces
    for zone in zoneinfo:
        zone.nsname = zone.name.rpartition('/')[0]
    nsnames = set(z.nsname for z in zoneinfo)

    # De-duplicate entries
    sorted_zones = sorted(zoneinfo, key=lambda z: z.nsname)
    for i, zone in enumerate(sorted_zones):
        for z2 in sorted_zones[i+1:]:
            alias = zone.location if z2.nsname == zone.nsname else zone.name
            if not z2.tzstr_alias and zone.tzstr == z2.tzstr:
                z2.tzstr_alias = alias
            if zone.transitions and not z2.transitions_alias and zone.transitions == z2.transitions:
                z2.transitions_alias = alias

    header.write('\n/* AREAS */\n')
    areas = sorted({n.partition('/')[0] for n in nsnames})
    for area in areas:
        header.write(f'''
namespace {area} {{
DEFINE_FSTR_LOCAL(area, "{area}")
DECLARE_FSTR_VECTOR(zones, Info)
}}''')

    header.write('\n\n/* ZONES */\n')

    for nsname in sorted(nsnames):
        ns = get_namespace(nsname)
        header.write(f'''
namespace {ns} {{''')
        for zone in zoneinfo:
            if zone.nsname == nsname:
                write_zone(header, zone)
        header.write(f'}} // namespace {ns}\n')

    header.write('''
} // namespace TZ
''')


    if not source:
        return

    source.write(intro + '''
#include "tzdata.h"

namespace TZ {
''')
    for area in areas:
        source.write(f'''namespace {area} {{
DEFINE_FSTR_VECTOR(zones, Info,
''')
        for zone in zoneinfo:
            if zone.area != area:
                continue
            ns = get_namespace(zone.location)
            source.write(f'\t&{ns}::info,\n')
        source.write(f''')
}} // namespace {area}

''')
    source.write('DEFINE_FSTR_MAP(areas, FSTR::String, ZoneList,\n')
    for area in ZONE_AREAS:
        source.write(f'\t{{&{area}::area, &{area}::zones}},\n')
    source.write(''')

} // namespace TZ
''')


def print_zones():
    zone_list = ZoneList()
    if not args.strings:
        zone_names = zone_list
    else:
        zone_names = set()
        for s in args.strings:
            matches = zone_list.find_matches(s)
            if not matches:
                raise RuntimeError(f"{s} doesn't match any known timezone names")
            zone_names |= set(matches)
        zone_names = sorted(zone_names)

    if args.name or args.tzstr or args.rule or args.transitions:
        zoneinfo = [get_info(n) for n in zone_names]
        write_zones(zoneinfo, sys.stdout, None)
    else:
        print("\n".join(zone_names))


def print_posix_rule():
    zoneinfo = [
        TimezoneInfo(name=f'Custom/Rule{i+1}', tzstr=s, rules=decode_tzstr(s), tzinfo=None)
        for i, s in enumerate(args.strings)]
    write_zones(zoneinfo, sys.stdout, None)


def dump_tzinfo():
    desigs = set()
    max_desig_len = 0
    for name in ZoneList():
        zone = get_info(name)
        max_desig_len = max(max_desig_len, len(zone.info.tznames))
        filename = os.path.join(args.output, zone.name)
        if not zone.transitions:
            try:
                os.remove(filename)
            except FileNotFoundError:
                pass
            continue
        os.makedirs(os.path.dirname(filename), exist_ok=True)
        with open(filename, 'wb') as f:
            print(zone.name)
            print(zone.info.tznames, zone.info.stdwall, zone.info.utlocal)
            for inf in zone.info.timetypes:
                print(inf.tzname, inf.indicator)
                desigs.add(inf.tzname)
            f.write(b''.join(t.pack() for t in zone.transitions))
    print(f'{len(desigs)} unique designators, total length {sum(len(d)+1 for d in desigs)}, max {max(len(d) for d in desigs)}, max list len {max_desig_len}')


def main():
    parser = ArgumentParser(description='Generate C++ source from compiled IANA database')
    parser.add_argument('--source', help='Optional path to compiled zoneinfo, overrides python zoneinfo settings')
    parser.add_argument('--name', action='store_true', help='Include zone area and name() method in TZ Info')
    parser.add_argument('--tzstr', action='store_true', help='Include POSIX timezone strings in TZ Info')
    parser.add_argument('--rule', action='store_true', help='Include decoded Rule definitions in TZ Info')
    parser.add_argument('--transitions', action='store_true', help='Include transition data')
    parser.add_argument('--from', type=int, default=1000, help='First year of interest')
    parser.add_argument('--to',  type=int, default=9999, help='Last year of interest')
    parser.set_defaults(func=None)
    subparsers = parser.add_subparsers()

    sub = subparsers.add_parser('zone', help='Lookup zone(s)')
    sub.add_argument('strings', nargs='*', help='zone name(s), e.g. "Europe/London", "europe/lon" or "eur" for multiple zones')
    sub.set_defaults(func=print_zones)

    sub = subparsers.add_parser('rule', help='Convert POSIX string(s) to rules')
    sub.add_argument('strings', nargs='+', help='POSIX rule string(s) to parse')
    sub.set_defaults(func=print_posix_rule)

    sub = subparsers.add_parser('full', help='Generate header and source code for database')
    sub.add_argument('output', help='Directory to write header/source files')
    sub.set_defaults(func=write_zones_full)

    sub = subparsers.add_parser('dump', help='Dump timezone files in minimal standard format')
    sub.add_argument('output', help='Directory to write files')
    sub.set_defaults(func=dump_tzinfo)

    global args
    args = parser.parse_args()

    if args.source:
        print('Set ZONEINFO_PATH')
        import tzdb
        # global ZONEINFO_PATH
        tzdb.ZONEINFO_PATH = args.source

    if args.func:
        args.func()
    else:
        parser.print_help()



if __name__ == '__main__':
    main()
