#!/usr/bin/python3
#
# Compile data into files
#
#

import os
from tzinfo import TzData, ZoneTable, TimeOffset, Zone, Link, remove_accents, TzString
from datetime import date, datetime, timedelta, timezone

OUTPUT_DIR = 'files'

def create_file(name: str):
    filename = os.path.join(OUTPUT_DIR, name)
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
    """
    for name, rules in tzdata.rules.items():
        with create_file(f'rules/{name}') as f:
            for rule in rules:
                f.write(f'R {name} {repr(rule)}\n')


    """
    Create separate .zi file for each region (area/continent)
    """
    regions = set([x.region for x in (tzdata.zones + tzdata.links)])
    for region in regions:
        with create_file(f'{region or "default"}.zi') as f:
            for zone in tzdata.zones:
                if zone.region != region:
                    continue
                f.write(f'Z {zone} ')
                for era in zone.eras:
                    f.write(f'{repr(era)}\n')
            for link in tzdata.links:
                if link.region == region:
                    f.write(f'L {link.zone.name} {link.name}\n')




def write_zonetab(zonetab: ZoneTable):
    """
    The two main output files are `countries` and `timezones`.
    These are compact versions of iso3166.tab and zone1970.tab.
    The first line contains field names so contents can be updated without affecting parsing.

    Entries are sorted in typical menu display order (captions) to simplify application code.

    For timezones, the caption is taken from 'comments' if present, and the last segment of
    the zone name if not. For example:

        ES\tEurope/Madrid\tSpain (mainland)
            caption is "Spain (mainland)"
        
        GE\tAsia/Tbilisi
            caption is "Tbilisi"

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
    tzdata = TzData()
    tzdata.load()
    write_tzdata(tzdata)

    zonetab = ZoneTable()
    zonetab.load(tzdata)
    write_zonetab(zonetab)


if __name__ == '__main__':
    main()
