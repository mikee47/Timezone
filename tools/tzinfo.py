#!/usr/bin/python3
#
# Decodes IANA timezone database into python structures which can then be emitted in various
# formats as required by applications.
#

import os
import sys
import json
import re
import unicodedata
from datetime import datetime, date, timezone, timedelta
import calendar
from dataclasses import dataclass, field

# Where to find IANA timezone database locally
ZONEINFO_PATH = '/usr/share/zoneinfo'

# Format described here https://data.iana.org/time-zones/data/zic.8.txt
TZDATA_PATH = ZONEINFO_PATH + '/tzdata.zi'
# Country data
COUNTRYTAB_PATH = ZONEINFO_PATH + '/iso3166.tab'
# Zone descriptions
ZONETAB_PATH = ZONEINFO_PATH + '/zone1970.tab'

MONTH_NAMES = [
    'January',
    'February',
    'March',
    'April',
    'May',
    'June',
    'July',
    'August',
    'September',
    'October',
    'November',
    'December',
]

DAY_NAMES = [
    'Sunday',
    'Monday',
    'Tuesday',
    'Wednesday',
    'Thursday',
    'Friday',
    'Saturday',
]

def match_name(values: list[str], name: str) -> str | None:
    for v in values:
        if v.startswith(name):
            return v
    return None

def match_month_name(name: str) -> str:
    month = match_name(MONTH_NAMES, name)
    if month:
        return month
    raise ValueError(f'Unknown month "{name}"')

def get_month_number(name: str) -> int:
    name = match_name(MONTH_NAMES, name)
    return 1 + MONTH_NAMES.index(name)

def match_day_name(name: str) -> str:
    day = match_name(DAY_NAMES, name)
    if day:
        return day
    raise ValueError(f'Unknown day "{name}"')

def get_day_number(name: str) -> int:
    name = match_name(DAY_NAMES, name)
    return DAY_NAMES.index(name)

def iso6709_string_to_float(s: str) -> float:
    def decode(sign, deg, min, sec) -> float:
        value = round(int(deg) + int(min) / 60.0 + int(sec) / 3600.0, 3)
        return -value if sign == '-' else value

    if len(s) == 11:
        lat = decode(s[0], s[1:3], s[3:5], 0)
        lng = decode(s[5], s[6:9], s[9:11], 0)
    elif len(s) == 15:
        lat = decode(s[0], s[1:3], s[3:5], s[5:7])
        lng = decode(s[7], s[8:11], s[11:13], s[13:15])
    else:
        raise ValueError('Invalid ISO6709 string "{s}"')
    return lat, lng


def remove_accents(s: str) -> str:
    return ''.join(c for c in unicodedata.normalize('NFD', s)
                                if unicodedata.category(c) != 'Mn')

def make_tag(s: str) -> str:
    class Table:
        def __getitem__(self, c: str):
            return '_' if chr(c) in '/ ' else c if chr(c).isalnum() else None
    s = remove_accents(s).translate(Table())
    return s.replace('__', '_')

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



@dataclass
class On:
    expr: str

    def __str__(self):
        return self.expr

    def get_date(self, year: int, month: str | int) -> date:
        """Get effective date for given year/month"""
        if isinstance(month, str):
            month = get_month_number(month)
        value = self.expr
        if value[0].isdigit():
            # 5        the fifth of the month
            return date(year, month, int(value))

        if value.startswith('last'):
            # lastSun  the last Sunday in the month
            # lastMon  the last Monday in the month
            weekday = get_day_number(value[4:])
            day = calendar.monthrange(year, month)[1]
            want_prev = True
        else:
            # Sun>=8   first Sunday on or after the eighth
            # Sun<=25  last Sunday on or before the 25th
            # The “<=” and “>=” constructs can result in a day in the neighboring month; for example, the
            # IN-ON combination “Oct Sun>=31” stands for the first Sunday on or after October 31,
            # even if that Sunday occurs in November.
            weekday = re.match('[a-zA-Z]+', value)[0]
            tail = value[len(weekday):]
            weekday = get_day_number(weekday)
            day = int(tail[2:])
            if tail.startswith('>='):
                want_prev = False
            elif tail.startswith('<='):
                want_prev = True
            else:
                raise ValueError(f'Malformed ON expression "{self.expr}"')

        d = date(year, month, day)
        wday = (d.weekday() + 1) % 7 # Start from Sunday, not Monday
        diff = weekday - wday
        if want_prev:
            if diff > 0:
                diff -= 7
        elif diff < 0:
            diff += 7
        # may extend outside starting month so use ordinal for calculation
        ordinal = d.toordinal() + diff
        return date.fromordinal(ordinal)


@dataclass
class At:
    hour: int = 0
    min: int = 0
    sec: int = 0
    timefmt: str = ''

    def __init__(self, s: str = None):
        if s is None:
            return
        self.timefmt = s[-1]
        if 'a' <= self.timefmt <= 'z':
            s = s[:-1]
        else:
            self.timefmt = ''
        fields = s.split(':')
        if fields:
            self.hour = int(fields.pop(0))
        if fields:
            self.min = int(fields.pop(0))
        if fields:
            self.sec = int(fields.pop(0))

    def __str__(self):
        return f'{self.hour:02d}:{self.min:02d}:{self.sec:02d}{self.timefmt}'

    @property
    def delta(self):
        return timedelta(hours=self.hour, minutes=self.min, seconds=self.sec)

    def adjust_utc(self, dt: datetime, stdoff: timedelta, dstoff: timedelta):
        delta = self.delta
        if self.timefmt in ['', 'w']:
            return dt + delta - stdoff - dstoff
        if self.timefmt == 's':
            return dt + delta - stdoff
        if self.timefmt in ['u', 'g', 'z']:
            return dt + delta
        raise ValueError(f'Invalid timefmt {self.timefmt}')


@dataclass
class Until:
    year: int = None
    month: str = 'Jan'
    day: On = None
    at: At = None

    def __init__(self, fields: list[str]):
        if fields:
            # YEAR
            self.year = int(fields.pop(0))
        if fields:
            # MONTH (Rule IN)
            self.month = match_month_name(fields.pop(0))[:3]
        if fields:
            # DAY (Rule ON)
            self.day = On(fields.pop(0))
        else:
            self.day = On('1')
        if fields:
            # TIME (Rule AT)
            self.at = At(fields.pop(0))
            assert(not fields)
        else:
            self.at = At()

    def __bool__(self):
        return self.year is not None

    def __str__(self):
        return f'{self.year}/{self.month}/{self.day} {str(self.at)}' if self else ''

    def get_date(self) -> date:
        if not self:
            return date(9999, 12, 31)
        return self.day.get_date(self.year, self.month)

    def applies_to(self, d: date) -> bool:
        return d is None or d <= self.get_date()


@dataclass
class PosixExpr:
    month: int = 0  # 1 <= month <= 12
    week: int = 0   # 1 <= week <= 5 (5 indicates 'last')
    day: int = 0    # 0=Sunday
    time: At = None

    def __init__(self, expr: str):
        m = re.match(r'M(\d+)\.(\d+)\.(\d+)/?(.+)?', expr)
        g = m.groups()
        self.month = int(g[0])
        self.week = int(g[1])
        self.day = int(g[2])
        self.time = At(g[3])

    def __str__(self):
        return f'{self.month}.{self.week}.{self.day}/{self.time}'


@dataclass
class TzString:
    std_name: str = ''
    std_offset: str = ''
    dst_name: str = ''
    dst_offset: str = '0'
    std_expr: PosixExpr = None
    dst_expr: PosixExpr = None

    def __init__(self, tzstr: str):
        assert(tzstr[0] != ':')

        m = re.match(r'''
            (<[^>]+>|[a-zA-Z]+)([\d:\-+]+)      # std offset
            (<[^>]+>|[a-zA-Z]+)?([\d:\-+]+)?    # [ dst [offset]
            ,?([^,]+)?,?([^,]+)?                # [ , date [ / time ] [,date[/time]] ]
            ''', tzstr, flags=re.VERBOSE)
        g = m.groups()
        self.std_name = g[0].strip('<>')
        self.std_offset = g[1]
        if g[2]:
            self.dst_name = g[2].strip('<>')
        if g[3]:
            self.dst_offset = g[3]
        if g[4]:
            self.std_expr = PosixExpr(g[4])
        if g[5]:
            self.dst_expr = PosixExpr(g[5])

    def __str__(self):
        return f'"{self.std_name}" {self.std_offset}, "{self.dst_name}" {self.dst_offset}, {self.std_expr}, {self.dst_expr}'


@dataclass
class Rule:
    from_: int # YEAR
    to: int    # YEAR
    in_: str   # MONTH
    on: On     # DAY
    at: At     # TIME of day
    save: At
    letters: str

    def __post_init__(self):
        if self.from_.startswith('mi'):
            self.from_ = 0
        else:
            self.from_ = int(self.from_)
        if self.to.startswith('o'):
            self.to = self.from_
        elif self.to.startswith('ma'):
            self.to = 9999
        else:
            self.to = int(self.to)
        self.in_ = match_month_name(self.in_)[:3]
        self.on = On(self.on)
        self.at = At(self.at)
        self.save = At(self.save)

    def __str__(self):
        return f'{self.from_} {self.to} - {self.in_} {self.on} {self.at} {self.save} {self.letters}'

    def applies_to(self, year_from: int, year_to: int = None) -> bool:
        if year_from is None:
            return True
        if self.from_ <= year_from <= self.to:
            return True
        if year_to is None:
            return False
        if self.from_ <= year_to <= self.to:
            return True
        return self.from_ >= year_from and self.to <= year_to
        

    def get_datetime_utc(self, year: int, stdoff: timedelta, dstoff: timedelta) -> datetime:
        """Obtain transition date/time in UTC"""
        d = self.on.get_date(year, self.in_)
        dt = datetime(d.year, d.month, d.day, tzinfo=timezone.utc)
        return self.at.adjust_utc(dt, stdoff, dstoff)


@dataclass
class ZoneRule:
    stdoff: At
    rule: str
    format: str
    until: Until

    def __post_init__(self):
        at = At(self.stdoff)
        assert(not at.timefmt)
        self.stdoff = at

    def __str__(self):
        s = f'{self.stdoff} {self.rule} {self.format}'
        if self.until:
            s += f' {self.until}'
        return s

    def applies_to(self, d: date) -> bool:
        return self.until.applies_to(d)


@dataclass
class NamedItem:
    name: str

    def __str__(self):
        return self.name

    def __lt__(self, other):
        return self.name < str(other)

    @property
    def region(self) -> str:
        return self.name.rpartition('/')[0]

    @property
    def zone_name(self) -> str:
        return self.name.rpartition('/')[2]

    @property
    def namespace(self) -> str:
        return make_namespace(self.region)

    @property
    def tag(self) -> str:
        table = str.maketrans({'-': '_N', '+': '_P'})
        return self.zone_name.translate(table)


@dataclass
class Zone(NamedItem):
    tzstr: str
    rules: list[ZoneRule] = field(default_factory=list)

    def add_rule(self, fields: list[str]):
        stdoff = fields.pop(0)
        rule = fields.pop(0)
        format = fields.pop(0)
        until = Until(fields)
        rule = ZoneRule(stdoff, rule, format, until)
        self.rules.append(rule)
        return rule

    def get_rules(self, d: date) -> list[ZoneRule]:
        return [zr for zr in self.rules if zr.applies_to(d)]


@dataclass
class Link(NamedItem):
    zone: Zone


class TzData:
    def __init__(self):
        self.comments = []
        self.rules: dict[list[Rule]] = {}
        self.zones: list[Zone | Link] = []

    def add_rule(self, fields: list[str]) -> Rule:
        name = fields[1]
        rule = Rule(fields[2], fields[3], fields[5], fields[6], fields[7], fields[8], fields[9])
        rules = self.rules.setdefault(name, [])
        rules.append(rule)
        return rule

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
        zone = Zone(name, tzstr)
        zone.add_rule(fields[2:])
        self.zones.append(zone)
        return zone

    def add_link(self, fields: list[str]):
        tgt_name, link_name = fields[1:3]
        if link_name in self.zones:
            print(f'{link_name} already specified, skipping')
            return
        zone = next(z for z in self.zones if z.name == tgt_name)
        link = Link(link_name, zone)
        self.zones.append(link)

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
                zone.add_rule(fields)
        self.zones.sort()

    def write_file(self, f, define: bool):
        d_from = date(2024, 1, 1)
        d_to = date(2034, 1, 1)

        f.write('\n')
        for c in self.comments:
            f.write(f'// {c}\n')
        zone_count = len([x for x in self.zones if isinstance(x, Zone)])
        link_count = len(self.zones) - zone_count
        f.write(f'// Contains {zone_count} zones and {link_count} links.\n')

        def get_delta(zone):
            if isinstance(zone, Link) or zone.region == 'Etc':
                return timedelta()
            return aggregator(zr.stdoff.delta for zr in zone.rules if zr.applies_to(d_from))

        aggregator = min
        zone = aggregator(self.zones, key=get_delta)
        offset = get_delta(zone)
        f.write(f'// {zone.name} has minimum std offset of -{-offset}.\n')

        aggregator = max
        zone = aggregator(self.zones, key=get_delta)
        offset = get_delta(zone)
        f.write(f'// {zone.name} has maximum std offset of {offset}.\n')

        f.write('\n')

        f.write('namespace TZ {\n')
        for region in sorted(set(z.region for z in self.zones)):
            if region:
                region_tag = 'TZREGION_' + make_tag(region)
                f.write(f'namespace {make_namespace(region)} {{\n')
            else:
                region_tag = 'TZREGION_NONE'
            if define:
                f.write(f'  DEFINE_FSTR_LOCAL({region_tag}, "{region}")\n')
            for zone in [z for z in self.zones if z.region == region]:
                indent = '   *'
                tag = zone.tag
                if define:
                    f.write(f'''
  /*
{indent} {zone.name}
''')
                    if isinstance(zone, Link):
                        link = zone
                        zone = link.zone
                        f.write(f'''{indent}/
  const TzInfo& {tag} PROGMEM = TZ::{zone.namespace}::{zone.tag};
''')
                        continue

                    f.write(f'{indent}   {TzString(zone.tzstr)}\n')

                    DATEFMT = '%Y %a %b %d %H:%M'
                    for zr in zone.get_rules(d_from):
                        f.write(f'{indent} {zr}\n')

                        rules = self.rules.get(zr.rule)
                        if rules:
                            dstoff = timedelta()
                            for r in rules:
                                if r.applies_to(d_from.year, d_to.year):
                                    f.write(f'{indent}   {r}\n')

                                    stdoff = zr.stdoff.delta
                                    for year in range(max(d_from.year, r.from_), min(d_to.year, r.to) + 1):
                                        dt = r.get_datetime_utc(year, stdoff, dstoff)
                                        f.write(f'{indent}     {(dt + stdoff + dstoff).strftime(DATEFMT)} {tztag}\n')
                                        # f.write(f'{indent}     {(dt + stdoff).ctime()} STD\n')
                                        # f.write(f'{indent}     {dt.ctime()} UTC\n')
                                    dstoff = r.save.delta

                                if '/' in zr.format:
                                    tztag = zr.format.split('/')[1 if r.save.delta else 0]
                                elif r.letters == '-':
                                    tztag = zr.format.replace('%s', '')
                                else:
                                    tztag = zr.format.replace('%s', r.letters)
                        elif zr.rule == '-':
                            pass
                        else:
                            f.write(f'{indent}  {zr.rule}\n')
                    f.write(f'''{indent}/
  DEFINE_FSTR_LOCAL(TZNAME_{tag}, "{zone.zone_name}")
  DEFINE_FSTR_LOCAL(TZSTR_{tag}, "{zone.tzstr}")
  const TzInfo {tag} PROGMEM {{
      .region = {region_tag},
      .name = TZNAME_{tag},
      .rules = TZSTR_{tag},
  }};
''')
                else:
                    ref = '&' if isinstance(zone, Link) else ''
                    f.write(f'  extern const TzInfo{ref} {tag};\n')
            if region:
                f.write(f'}} // namespace {make_namespace(region)}\n')
            f.write('\n')
        f.write('} // namespace TZ\n')

    def write_source(self, filename: str):
        with create_file(f'{filename}.h') as f:
            f.write('''
#pragma once

#include <Timezone.h>

struct TzInfo {
    const FSTR::String& region;
    const FSTR::String& name;
    const FSTR::String& rules;
    TimeChangeRule dst;
    TimeChangeRule std;

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
            self.write_file(f, False)

        with create_file(f'{filename}.cpp') as f:
            f.write(f'#include "{filename}.h"\n')
            self.write_file(f, True)


@dataclass
class TimeZone:
    country_codes: list[str]
    coordinates: str
    zone: Zone
    comments: str

    @property
    def caption(self):
        return self.comments or self.zone.zone_name

    def __lt__(self, other):
        return self.caption.lower() < other.caption.lower()

    @property
    def latlng(self):
        return iso6709_string_to_float(self.coordinates)


@dataclass
class Country:
    code: str
    name: str
    timezones: list[TimeZone] = None

    @property
    def sort_key(self):
        return remove_accents(self.name).lower()


@dataclass
class Continent:
    name: str
    countries: list[Country]

    @property
    def caption(self):
        """Return a slightly more user-friendly continent string, as given by `tzselect`"""
        name = self.name
        if name == 'America':
            return f'{name}s'
        if name in ['Arctic', 'Atlantic', 'Indian', 'Pacific']:
            return f'{name} Ocean'
        return name


    def __eq__(self, other):
        return self.name == (other if isinstance(other, str) else other.name)


@dataclass
class ZoneTable:
    continents: list[Continent] = None
    timezones: list[TimeZone] = None

    def load(self, tzdata):
        # COUNTRIES
        countries_by_code = {}
        for line in open(COUNTRYTAB_PATH):
            if line.startswith('#'):
                continue
            code, _, name = line.strip().partition('\t')
            countries_by_code[code] = Country(code, name)
        def country_key(c):
            return c.sort_key
        countries_by_name = dict((c.name, c) for c in sorted(countries_by_code.values(), key=country_key))

        # ZONES - naming as for `tzselect``
        self.timezones = []
        unique_regions = set()
        for line in open(ZONETAB_PATH):
            if line.startswith('#'):
                continue
            fields = line.strip().split('\t')
            country_codes = fields.pop(0).split(',')
            coordinates = fields.pop(0)
            tz = fields.pop(0)
            comments = fields.pop(0) if fields else ''
            zone = next(z for z in tzdata.zones if z.name == tz)
            self.timezones.append(TimeZone(country_codes, coordinates, zone, comments))

        # 1) Continent or Ocean
        self.continents = []
        continent_names = sorted(list(set(tz.zone.region.partition('/')[0]
                                        for tz in self.timezones)), key=str.lower)
        for continent_name in continent_names:
            codes = set()
            for tz in self.timezones:
                if tz.zone.name.startswith(continent_name):
                    codes |= set(tz.country_codes)
            # 2) Country
            countries = [c for c in countries_by_name.values() if c.code in codes]
            continent = Continent(continent_name, countries)
            self.continents.append(continent)
            for country in countries:
                # 3) Zone
                country.timezones = sorted([tz for tz in self.timezones if country.code in tz.country_codes])

    def write_file(self, f, define: bool):
        if define:
            unique_strings = set()
            for continent in self.continents:
                unique_strings.add(continent.name)
                for country in continent.countries:
                    unique_strings.add(country.name)
                    unique_strings |= set(tz.comments for tz in country.timezones)
            for s in sorted(list(unique_strings)):
                f.write(f'DEFINE_FSTR_LOCAL(STR_{make_tag(s)}, "{s}")\n')
            f.write('\n')

            for tz in self.timezones:
                tz_tag = 'tz_' + make_tag(tz.zone.name).lower()
                lat, lng = tz.latlng
                f.write(f'''
const TimeZone {tz_tag} PROGMEM {{
    {lat}, {lng},
    &TZ::{tz.zone.namespace}::{tz.zone.tag},
''')
                if tz.comments:
                    f.write(f'''\
    &STR_{make_tag(tz.comments)},
''')
                f.write('};\n')

        for continent in self.continents:
            continent_tag = make_tag(continent.name)
            for country in continent.countries:
                country_tag = make_tag(country.name)
                if define:
                    f.write(f'''
const Country {continent_tag}_{country_tag} PROGMEM {{
    "{country.code}",
    &STR_{country_tag},
}};
''')
                else:
                    f.write(f'extern const Country {continent_tag}_{country_tag};\n')
            if define:
                f.write(f'DEFINE_FSTR_VECTOR_LOCAL(COUNTRIES_{continent_tag}, Country')
                for country in continent.countries:
                    country_tag = make_tag(country.name)
                    f.write(f',\n  &{continent_tag}_{country_tag}')
                f.write(f''')

const Continent {continent_tag} PROGMEM {{
    &STR_{continent_tag},
    &COUNTRIES_{continent_tag},
}};
''')
            else:
                f.write(f'extern const Continent {continent_tag};\n')
            f.write('\n')

    def write_source(self, filename: str):
        with create_file(f'{filename}.h') as f:
            f.write('''
#pragma once

#include "tzdata.h"
#include <FlashString/Vector.hpp>

namespace TZ::Index
{

struct TimeZone {
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

struct Country {
    char code[4];
    const FlashString* name;
    const FSTR::Vector<TimeZone>* timezones;

    static constexpr const Country& empty()
    {
        return empty_;
    }

    static const Country empty_;
};

struct Continent {
    const FlashString* name;
    const FSTR::Vector<Country>* countries;

    String caption() const;
};

''')
            self.write_file(f, False)
            f.write('} // namespace TZ::Index')

        with create_file(f'{filename}.cpp') as f:
            f.write(f'#include "{filename}.h"\n')
            f.write('''

namespace TZ::Index
{
const Country Country::empty_{};

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
            self.write_file(f, True)
            f.write('} // namespace TZ::Index')

    def print(self):
        for icon, continent in enumerate(self.continents):
            print(f'{icon+1}) {continent.caption}')
            for icnt, country in enumerate(continent.countries):
                print(f'  {icon+1}.{icnt+1}) {country.name}')
                for it, tz in enumerate(country.timezones):
                    desc = f'{icon+1}.{icnt+1}.{it+1}) {tz.caption}'
                    if len(desc) > 40:
                        print(f'      {desc}')
                        desc = ''
                    print(f'      {desc:40} {tz.zone.name}')




def main():
    tzdata = TzData()
    tzdata.load()
    tzdata.write_source('tzdata')

    zonetab = ZoneTable()
    zonetab.load(tzdata)
    zonetab.write_source('tzindex')


if __name__ == '__main__':
    main()
