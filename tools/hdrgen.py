#!/usr/bin/python3
#
# Generate header and source code for linking zone information into application images
#

from tzinfo import TzData, ZoneTable, Zone, Link, remove_accents, TzString
from datetime import date, timedelta

def make_tag(s: str) -> str:
    class Table:
        def __getitem__(self, c: str):
            return '_' if chr(c) in '/ ' else c if chr(c).isalnum() else None
    s = remove_accents(s).translate(Table())
    return s.replace('__', '_').lower()


def make_zone_tag(zone: Zone) -> str:
    table = str.maketrans({'-': '_N', '+': '_P'})
    return zone.zone_name.translate(table)


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
        zone_count = len([x for x in tzdata.zones if isinstance(x, Zone)])
        link_count = len(tzdata.zones) - zone_count
        f.write(f'// Contains {zone_count} zones and {link_count} links.\n')

        def get_delta(zone):
            if isinstance(zone, Link) or zone.region == 'Etc':
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
        for region in sorted(set(z.region for z in tzdata.zones)):
            if region:
                region_tag = 'TZREGION_' + make_tag(region)
                f.write(f'namespace {make_namespace(region)} {{\n')
            else:
                region_tag = 'TZREGION_NONE'
            if define:
                f.write(f'  DEFINE_FSTR_LOCAL({region_tag}, "{region}")\n')
            for zone in [z for z in tzdata.zones if z.region == region]:
                indent = '   *'
                tag = make_zone_tag(zone)
                if define:
                    f.write(f'''
  /*
{indent} {zone.name}
''')
                    if isinstance(zone, Link):
                        link = zone
                        zone = link.zone
                        f.write(f'''{indent}/
  const TzInfo& {tag} PROGMEM = TZ::{make_namespace(zone.region)}::{make_zone_tag(zone)};
''')
                        continue

                    f.write(f'{indent}   {TzString(zone.tzstr)}\n')

                    DATEFMT = '%Y %a %b %d %H:%M'
                    for era in zone.get_eras(d_from):
                        f.write(f'{indent} {era}\n')

                        rules = tzdata.rules.get(era.rule)
                        if rules:
                            dstoff = timedelta()
                            for r in rules:
                                if r.applies_to(d_from.year, d_to.year):
                                    f.write(f'{indent}   {r}\n')

                                    stdoff = era.stdoff.delta
                                    for year in range(max(d_from.year, r.from_), min(d_to.year, r.to) + 1):
                                        dt = r.get_datetime_utc(year, stdoff, dstoff)
                                        f.write(f'{indent}     {(dt + stdoff + dstoff).strftime(DATEFMT)} {tztag}\n')
                                        # f.write(f'{indent}     {(dt + stdoff).ctime()} STD\n')
                                        # f.write(f'{indent}     {dt.ctime()} UTC\n')
                                    dstoff = r.save.delta

                                if '/' in era.format:
                                    tztag = era.format.split('/')[1 if r.save.delta else 0]
                                elif r.letters == '-':
                                    tztag = era.format.replace('%s', '')
                                else:
                                    tztag = era.format.replace('%s', r.letters)
                        elif era.rule == '-':
                            # Standard time applies
                            pass
                        else:
                            # A fixed amount of time (e.g. "1:00") added to standard time
                            f.write(f'{indent}  {era.rule}\n')
                    f.write(f'''{indent}/
  DEFINE_FSTR_LOCAL(TZNAME_{tag}, "{zone.zone_name}")
  DEFINE_FSTR_LOCAL(TZSTR_{tag}, "{zone.tzstr}")
  const TzInfo {tag} PROGMEM {{
      {region_tag},
      TZNAME_{tag},
      TZSTR_{tag},
  }};
''')
                else:
                    ref = '&' if isinstance(zone, Link) else ''
                    f.write(f'  extern const TzInfo{ref} {tag};\n')
            if region:
                f.write(f'}} // namespace {make_namespace(region)}\n')
            f.write('\n')
        f.write('} // namespace TZ\n')

    with create_file(f'{filename}.h') as f:
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
        write_file(f, False)

    with create_file(f'{filename}.cpp') as f:
        f.write(f'#include "{filename}.h"\n')
        write_file(f, True)



def write_zonetab(zonetab, filename: str):
    def write_file(f, define: bool):
        if define:
            unique_strings = set()
            for continent in zonetab.continents:
                unique_strings.add(continent.name)
                for country in continent.countries:
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
    &TZ::{make_namespace(tz.zone.region)}::{make_zone_tag(tz.zone)},
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

        for continent in zonetab.continents:
            continent_tag = make_tag(continent.name)
            if define:
                f.write(f'DEFINE_FSTR_VECTOR_LOCAL(countries_{continent_tag}, Country')
                for country in continent.countries:
                    country_tag = make_tag(country.name)
                    f.write(f',\n  &cnt_{country_tag}')
                f.write(f''')

const Continent {continent_tag} PROGMEM {{ {{}},
    &str_{continent_tag},
    &countries_{continent_tag},
}};
''')
            else:
                f.write(f'extern const Continent {continent_tag};\n')
            f.write('\n')

        if define:
            f.write('DEFINE_FSTR_VECTOR(continents, Continent')
            for continent in zonetab.continents:
                continent_tag = make_tag(continent.name)
                f.write(f',\n  &{continent_tag}')
            f.write(')\n')
        else:
            f.write('DECLARE_FSTR_VECTOR(continents, Continent)\n')


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

struct Continent: public Object<Continent>
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
String Continent::caption() const
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


def main():
    tzdata = TzData()
    tzdata.load()
    write_tzdata(tzdata, 'tzdata')

    zonetab = ZoneTable()
    zonetab.load(tzdata)
    write_zonetab(zonetab, 'tzindex')


if __name__ == '__main__':
    main()
