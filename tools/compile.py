#!/usr/bin/python3
#
# Compile data into files
#
#

import os
from tzinfo import TzData, ZoneTable, TimeOffset, Zone, Link, remove_accents, TzString
from datetime import date, datetime, timedelta, timezone

def write_tzdata(tzdata: TzData, filename: str):
    """
    Expand all rules inside zone descriptions, see what file size looks like
    """
    with open(filename, 'w') as f:
        for name, rules in tzdata.rules.items():
            for rule in rules:
                f.write(f'R {name} {repr(rule)}\n')
        for zone in tzdata.zones:
            f.write(f'Z {zone} ')
            for era in zone.eras:
                f.write(f'{repr(era)}\n')
        for link in tzdata.links:
            f.write(f'L {link.zone.name} {link.name}\n')



def write_zonetab(zonetab: ZoneTable, output_path: str):
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
    os.makedirs(output_path, exist_ok=True)

    # Content of zone1970.tab without comments or coordinates, first line column names
    with open(os.path.join(output_path, 'timezones'), 'w') as f:
        f.write('codes\tzone\tcomment\n')
        for tz in sorted(zonetab.timezones):
            f.write(f'{",".join(tz.country_codes)}')
            # f.write(f'\t{tz.coordinates}')
            f.write(f'\t{tz.zone.name}')
            if tz.comments:
                f.write(f'\t{tz.comments}')
            f.write('\n')

    with open(os.path.join(output_path, 'countries'), 'w') as f:
        f.write('code\tname\n')
        for c in zonetab.countries:
            f.write(f'{c.code}\t{c.name}\n')

def main():
    tzdata = TzData()
    tzdata.load()
    write_tzdata(tzdata, 'files/tzdata.zi')

    zonetab = ZoneTable()
    zonetab.load(tzdata)
    write_zonetab(zonetab, 'files')


if __name__ == '__main__':
    main()
