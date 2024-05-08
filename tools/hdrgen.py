#!/usr/bin/python3
#
# Generate header and source code for linking zone information into application images
#

from tzinfo import TzData, ZoneTable, TimeOffset, Zone, Link, remove_accents, TzString
from datetime import date, datetime, timedelta, timezone

def make_tag(s: str) -> str:
    class Table:
        def __getitem__(self, c: str):
            return '_' if chr(c) in '/ ' else c if chr(c).isalnum() else None
    s = remove_accents(s).translate(Table())
    return s.replace('__', '_').lower()


def make_zone_tag(zone: Zone) -> str:
    table = str.maketrans({'-': '_N', '+': '_P'})
    return zone.location.translate(table)


def make_namespace(s: str) -> str:
    return s.replace('/', '::')


def create_file(filename: str):
    f = open(filename, 'w')
    f.write(f'''\
/*
 * This file is auto-generated.
 */

// clang-format off
''')
    return f



def write_tzdata(tzdata, filename: str):
    def write_file(f, define: bool):
        d_from = date(2024, 1, 1)
        d_to = date(2034, 1, 1)

        f.write('\n')
        for c in tzdata.comments:
            f.write(f'// {c}\n')
        f.write(f'// Contains {len(tzdata.zones)} zones and {len(tzdata.links)} links.\n')

        def get_delta(zone):
            if zone.area == 'Etc':
                return timedelta()
            return aggregator(era.stdoff.delta for era in zone.eras if era.applies_to(d_from))

        aggregator = min
        zone = aggregator(tzdata.zones, key=get_delta)
        offset = get_delta(zone)
        f.write(f'// {zone.name} has minimum std offset of -{-offset}.\n')

        aggregator = max
        zone = aggregator(tzdata.zones, key=get_delta)
        offset = get_delta(zone)
        f.write(f'// {zone.name} has maximum std offset of {offset}.\n')

        f.write('\n')

        f.write('namespace TZ {\n')
        for area in sorted(set(z.area for z in tzdata.zones)):
            if area:
                area_tag = 'TZAREA_' + make_tag(area)
                f.write(f'namespace {make_namespace(area)} {{\n')
            else:
                area_tag = 'TZAREA_NONE'
            if define:
                f.write(f'  DEFINE_FSTR_LOCAL({area_tag}, "{area}")\n')

            for zone_or_link in (tzdata.zones + tzdata.links):
                if zone_or_link.area != area:
                    continue
                indent = '   *'
                tag = make_zone_tag(zone_or_link)
                if not define:
                    ref = '&' if isinstance(zone_or_link, Link) else ''
                    f.write(f'  extern const TzInfo{ref} {tag};\n')
                    continue

                f.write(f'''
  /*
{indent} {zone_or_link.name}
''')
                if isinstance(zone_or_link, Link):
                    link = zone_or_link
                    zone = link.zone
                    f.write(f'''{indent}/
  const TzInfo& {tag} PROGMEM = TZ::{make_namespace(zone.area)}::{make_zone_tag(zone)};
''')
                    continue

                zone = zone_or_link
                tzstr = TzString(zone.tzstr)
                f.write(f'{indent}   {tzstr}\n')
                f.write(f'{indent}   {repr(tzstr)}\n')

                DATEFMT = '%Y %a %b %d'
                DATETIMEFMT = DATEFMT + ' %H:%M'
                prev_era_end = d_from - timedelta(seconds=1)
                for era in zone.eras:
                    if not era.applies_to(d_from):
                        prev_era_end = era.until.get_date()
                        continue

                    f.write(f'{indent} {era}\n')

                    if era.rule == '-':
                        # Standard time applies
                        pass
                    elif rules := tzdata.rules.get(era.rule):
                        for r in rules:
                            if not r.applies_to(d_from.year, d_to.year):
                                continue
                            f.write(f'{indent}   {r}\n')

                            tztag = era.get_tag(r)

                            for year in range(max(d_from.year, r.from_), min(d_to.year, r.to) + 1):
                                d = r.get_date(year)
                                if d <= prev_era_end:
                                    continue
                                if not era.applies_to(d):
                                    break
                                dt = datetime(d.year, d.month, d.day, tzinfo=timezone.utc)
                                dt += r.at.delta
                                if r.at.timefmt == 's':
                                    dt -= era.stdoff.delta
                                    dts_new = dt.strftime(DATETIMEFMT) + ' ' + tztag
                                elif r.at.timefmt in ['u', 'g', 'z']:
                                    dt += era.stdoff.delta
                                    dt += r.save.delta
                                    dts_new = dt.strftime(DATETIMEFMT) + ' ' + tztag
                                elif r.save.is_dst:
                                    # Current time is STD
                                    dt += r.save.delta
                                    dts_new = dt.strftime(DATETIMEFMT) + ' ' + tztag
                                else:
                                    # TODO: Current time is DST, so we need to find the offset
                                    dts_new = ''

                                dts = d.strftime(DATEFMT) + f' {r.at}'

                                f.write(f'{indent}     {dts} -> {dts_new}\n')
                    else:
                        # A fixed amount of time (e.g. "1:00") added to standard time
                        offset = TimeOffset(era.rule)
                        f.write(f'{indent}  {offset}\n')
                    prev_era_end = era.until.get_date()

                f.write(f'''{indent}/
  DEFINE_FSTR_LOCAL(TZNAME_{tag}, "{zone.location}")
  DEFINE_FSTR_LOCAL(TZSTR_{tag}, "{zone.tzstr}")
  const TzInfo {tag} PROGMEM {{
      {area_tag},
      TZNAME_{tag},
      TZSTR_{tag},
  }};
''')
            if area:
                f.write(f'}} // namespace {make_namespace(area)}\n')
            f.write('\n')
        f.write('} // namespace TZ\n')

    with create_file(f'{filename}.h') as f:
        f.write('''
#pragma once

#include <WString.h>

struct TzInfo {
    const FSTR::String& area;
    const FSTR::String& name;
    const FSTR::String& rules;

    String fullName() const
    {
        String s;
        if(area.length() != 0) {
            s += area;
            s += '/';
        }
        s += name;
        return s;
    }

};

''')
        write_file(f, False)

    with create_file(f'{filename}.cpp') as f:
        f.write(f'#include "{filename}.h"\n')
        write_file(f, True)



def write_zonetab(zonetab, filename: str):
    def write_file(f, define: bool):
        if define:
            unique_strings = set()
            for area in zonetab.areas:
                unique_strings.add(area.name)
                for country in area.countries:
                    unique_strings.add(country.name)
                    unique_strings |= set(tz.comments for tz in country.timezones)
            for s in sorted(list(unique_strings)):
                f.write(f'DEFINE_FSTR_LOCAL(str_{make_tag(s)}, "{s}")\n')
            f.write('\n')

            for tz in zonetab.timezones:
                tz_tag = 'tz_' + make_tag(tz.zone.name)
                lat, lng = tz.latlng
                comments = f'&str_{make_tag(tz.comments)}' if tz.comments else 'nullptr'
                f.write(f'''
const TimeZone {tz_tag} PROGMEM {{ {{}},
    {lat}, {lng},
    &TZ::{make_namespace(tz.zone.area)}::{make_zone_tag(tz.zone)},
    {comments},
''')
                f.write('};\n')

        for country in zonetab.countries:
            if country.timezones is None:
                print(f'{country.name} has no timezones')
                continue
            country_tag = make_tag(country.name)
            if define:
                f.write(f'DEFINE_FSTR_VECTOR_LOCAL(timezones_{country_tag}, TimeZone')
                for tz in country.timezones:
                    tz_tag = 'tz_' + make_tag(tz.zone.name)
                    f.write(f',\n  &{tz_tag}')
                f.write(f''')

const Country cnt_{country_tag} PROGMEM {{ {{}},
    "{country.code}",
    &str_{country_tag},
    &timezones_{country_tag},
}};
''')
            else:
                f.write(f'extern const Country cnt_{country_tag};\n')

        for area in zonetab.areas:
            area_tag = make_tag(area.name)
            if define:
                f.write(f'DEFINE_FSTR_VECTOR_LOCAL(countries_{area_tag}, Country')
                for country in area.countries:
                    country_tag = make_tag(country.name)
                    f.write(f',\n  &cnt_{country_tag}')
                f.write(f''')

const Area {area_tag} PROGMEM {{ {{}},
    &str_{area_tag},
    &countries_{area_tag},
}};
''')
            else:
                f.write(f'extern const Area {area_tag};\n')
            f.write('\n')

        if define:
            f.write('DEFINE_FSTR_VECTOR(areas, Area')
            for area in zonetab.areas:
                area_tag = make_tag(area.name)
                f.write(f',\n  &{area_tag}')
            f.write(')\n')
        else:
            f.write('DECLARE_FSTR_VECTOR(areas, Area)\n')


    with create_file(f'{filename}.h') as f:
        f.write('''
#pragma once

#include "tzdata.h"
#include <FlashString/Vector.hpp>

namespace TZ::Index
{

template <class T> struct Object
{
    static constexpr const T& empty()
    {
        return empty_;
    }

    static const T empty_;
};

template <class T> const T Object<T>::empty_{};

struct TimeZone: public Object<TimeZone>
{
    // const FlashString* countryCodes;
    float lat;
    float lng;
    const TzInfo* _info;
    const FlashString* _comments;

    const TzInfo& info() const
    {
        return *_info;
    }

    String comments() const
    {
        return _comments ? String(*_comments) : String::empty;
    }

    String caption() const
    {
        return _comments ? *_comments : _info->name;
    }
};

struct Country: public Object<Country>
{
    char code[4];
    const FlashString* name;
    const FSTR::Vector<TimeZone>* timezones;
};

struct Area: public Object<Area>
{
    const FlashString* name;
    const FSTR::Vector<Country>* countries;

    String caption() const;
};

''')
        write_file(f, False)
        f.write('} // namespace TZ::Index\n')

    with create_file(f'{filename}.cpp') as f:
        f.write(f'#include "{filename}.h"\n')
        f.write('''


namespace TZ::Index
{
String Area::caption() const
{
    String s(*name);
    if(s == _F("America")) {
        s += 's';
    } else if(strchr("AIP", s[0]) && strchr("rtna", s[1])) {
        s += _F(" Ocean");
    }
    return s;
}
''')
        write_file(f, True)
        f.write('} // namespace TZ::Index\n')


TZDATA_PATH = '/stripe/sandboxes/tzdata/tzdb-2024a'

def main():
    tzdata = TzData()
    tzdata.load_full(TZDATA_PATH)
    write_tzdata(tzdata, 'tzdata')

    zonetab = ZoneTable()
    zonetab.load(tzdata, TZDATA_PATH)
    write_zonetab(zonetab, 'tzindex')


if __name__ == '__main__':
    main()
