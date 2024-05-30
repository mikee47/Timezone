#!/usr/bin/python3
#
# Compile data into files
#
#

from __future__ import annotations
import os
from tzinfo import TzData, ZoneTable, TimeOffset, Zone, Link, remove_accents, TzString, YEAR_MIN, YEAR_MAX
from datetime import date, datetime, timedelta, timezone
from argparse import ArgumentParser

output_dir = 'files'
year_from = YEAR_MIN
year_to = YEAR_MAX


def create_file(name: str):
    filename = os.path.join(output_dir, name)
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    # print(f'Writing "{filename}"...')
    return open(filename, 'w')


def write_tzdata_full(tzdata: TzData):
    """For checking against original tzdata.zi file"""
    with create_file('tzdata.zi') as f:
        for name, rules in tzdata.rules.items():
            for rule in rules:
                f.write(f'R {name} {repr(rule)}\n')
        for zone in tzdata.zones:
            f.write(f'Z {zone} ')
            for era in zone.eras:
                f.write(f'{repr(era)}\n')
        for link in tzdata.links:
            f.write(f'L {link.zone.name} {link.name}\n')


def write_tzdata(tzdata: TzData):
    """
    Put rules into separate files in the 'rules' directory

    SPEC CHANGE: Blank leading 'R' and name fields as these are the same for all entries
    SPEC CHANGE: Filter pre-1970's rules
    """
    for name, rules in tzdata.rules.items():
        filtered_rules = [r for r in rules if r.applies_to(year_from, year_to)]
        if filtered_rules:
            with create_file(f'rules/{name}') as f:
                f.write("".join(f'  {repr(r)}\n' for r in filtered_rules))


    """
    Create separate .zi file for each area

    SPEC CHANGE: Put initial era (continuation data) onto separate line
    SPEC CHANGE: Don't store area, it's the same for every entry in file
    """
    areas = set([x.area for x in (tzdata.zones + tzdata.links)])
    for area in areas:
        with create_file(f'{area or "default"}.zi') as f:
            for link in tzdata.links:
                if link.area == area:
                    f.write(f'L {link.zone.name} {link.location}\n')
            for zone in tzdata.zones:
                if zone.area != area:
                    continue
                f.write(f'Z {zone.location}\n')
                for era in zone.eras:
                    if era.until.get_date() >= date(year_from, 1, 1):
                        f.write(f'{repr(era)}\n')




def write_zonetab(zonetab: ZoneTable):
    """
    Output files without comments.

    Entries are sorted in typical menu display order (captions) to simplify application code.
    The timezone caption is taken from 'comments' if present, and the last segment of
    the zone name if not. For example:

        ES\tEurope/Madrid\tSpain (mainland)
            caption is "Spain (mainland)"
        
        GE\tAsia/Tbilisi
            caption is "Tbilisi"

    SPEC CHANGE: Omit co-ordinates from zone1970 (blank field) saves 4-5K
    """
    # Content of zone1970.tab without coordinates (saves 4-5K)
    with create_file('zone1970.tab') as f:
        for tz in sorted(zonetab.timezones):
            f.write(f'{",".join(tz.country_codes)}')
            f.write('\t') # f.write(f'\t{tz.coordinates}')
            f.write(f'\t{tz.zone.name}')
            if tz.comments:
                f.write(f'\t{tz.comments}')
            f.write('\n')

    with create_file('iso3166.tab') as f:
        for c in zonetab.countries:
            f.write(f'{c.code}\t{c.name}\n')


def compare_tzdata(old: TzData, new: TzData) -> TzData:
    """Return a TzData object containing only updated information
    Applications would store this in a writeable filesystem 'updates' directory
    and check before the base version.
    """

    # TODO: Apply year filter to dataset


    # @dataclass
    # class Stat:
    #     values: list = None
    #     count: int = 0

    tzupdate = TzData()

    # Scan rules
    rules_deleted = []
    deleted_rule_size = 0
    rules_changed = []
    changed_rule_size = 0
    for name, old_rules in old.rules.items():
        new_rules = new.rules.get(name)
        if not new_rules:
            print(f'Rules "{name}" deleted')
            rules_deleted.append(old_rules)
            deleted_rule_size += len(old_data)
        old_data = repr(old_rules)
        new_data = repr(new_rules)
        if new_data == old_data:
            continue
        print(f'Rules "{name}" changed, {len(new_data)} bytes')
        tzupdate.rules[name] = new_rules
        rules_changed.append(new_rules)
        changed_rule_size += len(new_data)
    rules_added = []
    added_rule_size = 0
    for name, new_rules in new.rules.items():
        if name in old.rules:
            continue
        new_data = repr(new_rules)
        print(f'Rules "{name}" added, {len(new_data)} bytes')
        tzupdate.rules[name] = new_rules
        rules_added.append(new_rules)
        added_rule_size += len(new_data)

    # Scan zones
    zones_deleted = []
    deleted_zone_size = 0
    zones_changed = []
    changed_zone_size = 0
    zones_added = []
    for zone in old.zones:
        old_data = repr(zone.eras)
        try:
            new_zone = next(z for z in new.zones if z == zone)
        except StopIteration:
            print(f'Zone "{zone}" deleted')
            zones_deleted.append(zone)
            deleted_zone_size += len(old_data)
            continue
        new_data = repr(new_zone.eras)
        if old_data == new_data:
            continue
        print(f'Zone "{zone}" changed, {len(new_data)}, bytes')
        tzupdate.zones.append(new_zone)
        zones_changed.append(new_zone)
        changed_zone_size += len(new_data)

    print('Change summary:')
    print(f'  {len(zones_deleted)} zones deleted, {deleted_zone_size} bytes could be released')
    print(f'  {len(rules_deleted)} rules deleted, {deleted_rule_size} bytes could be released')
    print(f'  {len(zones_changed)} zones changed, {changed_zone_size} bytes required')
    print(f'  {len(rules_changed)} rules changed, {changed_rule_size} bytes required')
    print(f'  {len(rules_added)} rules added, {added_rule_size} bytes required')

    return tzupdate


def main():
    parser = ArgumentParser(description='IANA database compiler')
    parser.add_argument('source', help='Path to TZ Database source')
    parser.add_argument('dest', help='Directory to write output files')
    parser.add_argument('--compare', help='Directory to write output files')
    args = parser.parse_args()

    global output_dir
    output_dir = args.dest

    tzdata = TzData()
    tzdata.load(args.source)

    if args.compare:
        prev = TzData()
        prev.load(args.compare)
        tzdata = compare_tzdata(prev, tzdata)
        write_tzdata(tzdata)
        return

    write_tzdata(tzdata)
    if os.path.isdir(args.source):
        zonetab = ZoneTable()
        zonetab.load(tzdata, args.source)
        write_zonetab(zonetab)


if __name__ == '__main__':
    main()
