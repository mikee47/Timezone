#!/usr/bin/python3
#
# Compile data into files
#
#

import os
from tzinfo import TzData, ZoneTable, TimeOffset, Zone, Link, remove_accents, TzString, YEAR_MIN, YEAR_MAX
from datetime import date, datetime, timedelta, timezone
from argparse import ArgumentParser

output_dir = 'files'

def create_file(name: str):
    filename = os.path.join(output_dir, name)
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    # print(f'Writing "{filename}"...')
    return open(filename, 'w')


def write_tzdata_full(tzdata: TzData, output_dir: str):
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


def write_tzdata(tzdata: TzData, year_from=YEAR_MIN, year_to=YEAR_MAX):
    """
    Put rules into separate files in the 'rules' directory

    SPEC CHANGE: Omit leading 'R' and name fields as these are the same for all entries
    SPEC CHANGE: Filter pre-1970's rules
    """
    for name, rules in tzdata.rules.items():
        filtered_rules = [r for r in rules if r.applies_to(year_from, year_to)]
        if filtered_rules:
            with create_file(f'rules/{name}') as f:
                f.write("".join(f'{repr(r)}\n' for r in filtered_rules))


    """
    Create separate .zi file for each area

    SPEC CHANGE: Put initial era (continuation data) onto separate line
    SPEC CHANGE: Don't store area, it's the same for every entry in file
    """
    areas = set([x.area for x in (tzdata.zones + tzdata.links)])
    for area in areas:
        with create_file(f'{area or "default"}.zi') as f:
            for zone in tzdata.zones:
                if zone.area != area:
                    continue
                f.write(f'Z {zone.location}\n')
                for era in zone.eras:
                    if era.until.get_date() >= date(year_from, 1, 1):
                        f.write(f'{repr(era)}\n')
            for link in tzdata.links:
                if link.area == area:
                    f.write(f'L {link.zone.name} {link.location}\n')




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



def main():
    parser = ArgumentParser(description='IANA database compiler')
    parser.add_argument('source', help='Path to TZ Database source')
    parser.add_argument('dest', help='Directory to write output files')
    args = parser.parse_args()

    global output_dir
    output_dir = args.dest

    tzdata = TzData()
    tzdata.load_full(args.source)
    write_tzdata(tzdata)

    zonetab = ZoneTable()
    zonetab.load(tzdata, args.source)
    write_zonetab(zonetab)


if __name__ == '__main__':
    main()
