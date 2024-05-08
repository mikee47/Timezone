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
from datetime import datetime, date, timedelta
import calendar
from dataclasses import dataclass, field

# Where to find compiled IANA timezone database locally
ZONEINFO_PATH = '/usr/share/zoneinfo'

# Primary database source files
TZDATA_FILE_LIST = [
    'africa',
    'antarctica',
    'asia',
    'australasia',
    'etcetera',
    'europe',
    'factory',
    'northamerica',
    'southamerica',
    'backward',
]

# Format described here https://data.iana.org/time-zones/data/zic.8.txt
TZDATA_ZI = 'tzdata.zi'
# Country data
COUNTRYTAB_FILENAME = 'iso3166.tab'
# Zone descriptions
ZONETAB_FILENAME = 'zone1970.tab'

# Values for 'min' and 'max' in data
YEAR_MIN = 1
YEAR_MAX = 9999

MONTH_NAMES = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec']
MONTH_NAMES_BRIEF = ['Ja', 'F', 'Mar', 'Ap', 'May', 'Jun', 'Jul', 'Au', 'S', 'O', 'N', 'D']
DAY_NAMES = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat']
DAY_NAMES_BRIEF = ['Su', 'M', 'Tu', 'W', 'Th', 'F', 'Sa']

def match_name(values: list[str], name: str) -> str | None:
    for v in values:
        if v.startswith(name):
            return v
    raise ValueError(f'Unknown "{name}"')

def get_month_index(name: str) -> int:
    name = match_name(MONTH_NAMES, name)
    return MONTH_NAMES.index(name)

def get_day_index(name: str) -> int:
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


class Month:
    def __init__(self, value: str | int):
        self.index = value if isinstance(value, int) else get_month_index(value)

    def __bool__(self):
        return self.index != 0

    def __str__(self):
        return MONTH_NAMES[self.index]

    def __repr__(self):
        return MONTH_NAMES_BRIEF[self.index]
        

@dataclass
class On:
    expr: str

    def __str__(self):
        return self.expr

    def __repr__(self):
        return self.expr

    def __bool__(self):
        return self.expr not in ['', '1']

    def get_date(self, year: int, month: str | int) -> date:
        """Get effective date for given year/month"""
        if isinstance(month, str):
            month = Month(month)
        if isinstance(month, Month):
            month = month.index + 1
        value = self.expr
        if value[0].isdigit():
            # 5        the fifth of the month
            return date(year, month, int(value))

        if value.startswith('last'):
            # lastSun  the last Sunday in the month
            # lastMon  the last Monday in the month
            weekday = get_day_index(value[4:])
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
            weekday = get_day_index(weekday)
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
    timefmt: str = 'w'

    def __init__(self, s: str = None):
        if s is None:
            return
        timefmt = s[-1]
        if 'a' <= timefmt <= 'z':
            s = s[:-1]
        else:
            timefmt = 'w'
        self.timefmt = timefmt
        fields = s.split(':')
        if fields:
            self.hour = int(fields.pop(0))
            assert(self.hour >= 0)
        if fields:
            self.min = int(fields.pop(0))
        if fields:
            self.sec = int(fields.pop(0))

    def __str__(self):
        return f'{self.hour}:{self.min:02}:{self.sec:02}{self.timefmt}'

    def __bool__(self):
        return self.hour != 0 or self.min != 0 or self.sec != 0 or self.timefmt != 'w'

    def __repr__(self):
        s = str(self.hour)
        if self.min or self.sec:
            s += f':{self.min}'
        if self.sec:
            s += f':{self.sec}'
        if self.timefmt != 'w':
            s += self.timefmt
        return s


    @property
    def delta(self):
        return timedelta(hours=self.hour, minutes=self.min, seconds=self.sec)


@dataclass
class TimeOffset:
    seconds: int = 0
    is_dst: bool = False

    def __init__(self, s: str = None):
        if s is None:
            return
        timefmt = s[-1]
        if 'a' <= timefmt <= 'z':
            s = s[:-1]
        else:
            timefmt = None
        fields = s.split(':')
        s = fields.pop(0)
        if s[0] == '-':
            sign = -1
            s = s[1:]
        else:
            sign = 1
        hours = int(s);
        mins = int(fields.pop(0)) if fields else 0
        secs = int(fields.pop(0)) if fields else 0
        seconds = (hours * 3600) + (mins * 60) + secs
        self.seconds = seconds * sign
        self.is_dst = (timefmt == 'd') if timefmt else (seconds != 0)

    @property
    def delta(self):
        return timedelta(seconds=self.seconds)

    def __bool__(self):
        return self.seconds != 0

    def __str__(self):
        delta = timedelta(seconds=abs(self.seconds))
        sign = '' if self.seconds >= 0 else '-'
        return f'{sign}{delta}'

    def __repr__(self):
        secs = self.seconds
        if secs < 0:
            sign = '-'
            secs = -secs
        else:
            sign = ''
        mins = secs // 60
        hours = mins // 60
        mins = mins % 60
        secs = secs % 60
        s = f'{sign}{hours}'
        if mins or secs:
            s += f':{mins}'
        if secs:
            s += f':{secs}'
        return s


@dataclass
class Until:
    year: int
    month: Month
    day: On
    at: At

    def __init__(self, fields: list[str]):
        # YEAR
        self.year = int(fields.pop(0)) if fields else None
        # MONTH (Rule IN)
        self.month = Month(fields.pop(0) if fields else 0)
        # DAY (Rule ON)
        self.day = On(fields.pop(0) if fields else '1')
        # TIME (Rule AT)
        self.at = At(fields.pop(0) if fields else None)
        assert(not fields)

    def __bool__(self):
        return self.year is not None

    def __str__(self):
        return f'{self.year}/{self.month}/{self.day} {str(self.at)}' if self else ''

    def __repr__(self):
        if self.year is None:
            return ''
        s = str(self.year)
        if self.month or self.day or self.at:
            s += f' {repr(self.month)}'
        if self.day or self.at:
            s += f' {repr(self.day)}'
        if self.at:
            s += f' {repr(self.at)}'
        return s

    def get_date(self) -> date:
        if not self:
            return date(YEAR_MAX, 12, 31)
        return self.day.get_date(self.year, self.month)

    def applies_to(self, d: date) -> bool:
        return d is None or d <= self.get_date()


@dataclass
class PosixExpr:
    month: Month
    week: int   # 1 <= week <= 5 (5 indicates 'last')
    day: int    # 0=Sunday
    time: TimeOffset = None # POSIX says non-negative, but Scoresbysund and Nuuk both violate this

    def __init__(self, expr: str):
        m = re.match(r'M(\d+)\.(\d+)\.(\d+)/?(.+)?', expr)
        g = m.groups()
        self.month = Month(int(g[0]) - 1)
        self.week = int(g[1])
        self.day = int(g[2])
        self.time = TimeOffset(g[3])

    def __str__(self):
        weekstr = "last" if self.week == 5 else f'week{self.week}'
        return f'{self.month}.{weekstr}.{DAY_NAMES[self.day]}/{self.time}'

    def __repr__(self):
        s = f'M{self.month.index+1}.{self.week}.{self.day}'
        if self.time:
            s += f'/{repr(self.time)}'
        return s


@dataclass
class TzString:
    std_name: str = ''
    std_offset: TimeOffset = None
    dst_name: str = ''
    dst_offset: TimeOffset = None
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
        self.std_offset = TimeOffset(g[1])
        if g[2]:
            self.dst_name = g[2].strip('<>')
        if g[3]:
            self.dst_offset = TimeOffset(g[3])
        if g[4]:
            self.std_expr = PosixExpr(g[4])
        if g[5]:
            self.dst_expr = PosixExpr(g[5])

    def __str__(self):
        return f'"{self.std_name}" {self.std_offset}, "{self.dst_name}" {self.dst_offset}, {self.std_expr}, {self.dst_expr}'

    def __repr__(self):
        def esc(s: str) -> str:
            return f'<{s}>' if s[0] in '+-' else s
        s = f'"{esc(self.std_name)}{repr(self.std_offset)}'
        if self.dst_name:
            s += esc(self.dst_name)
            if self.dst_offset:
                s += repr(self.dst_offset)
        if self.std_expr:
            s += f',{repr(self.std_expr)}'
        if self.dst_expr:
            assert self.std_expr
            s += f',{repr(self.dst_expr)}'
        return s


@dataclass
class Rule:
    from_: int # YEAR
    to: int    # YEAR
    in_: Month # MONTH
    on: On     # DAY
    at: At     # TIME of day
    save: TimeOffset
    letters: str

    def __init__(self, fields: list[str]):
        from_ = fields.pop(0)
        if from_.startswith('mi'):
            self.from_ = YEAR_MIN
        else:
            self.from_ = int(from_)

        to = fields.pop(0)
        if to.startswith('o'):
            self.to = self.from_
        elif to.startswith('ma'):
            self.to = YEAR_MAX
        else:
            self.to = int(to)

        # Unused
        _ = fields.pop(0)

        self.in_ = Month(fields.pop(0))
        self.on = On(fields.pop(0))
        self.at = At(fields.pop(0))
        self.save = TimeOffset(fields.pop(0))
        self.letters = fields.pop(0)

    def __str__(self):
        return f'{self.from_} {self.to} - {self.in_} {self.on} {self.at} {self.save} {self.letters}'

    def __repr__(self):
        from_ = 'mi' if self.from_ == YEAR_MIN else self.from_
        to = 'o' if self.to == self.from_ else 'ma' if self.to == YEAR_MAX else self.to
        return f'{from_} {to} - {repr(self.in_)} {self.on} {repr(self.at)} {repr(self.save)} {self.letters}'


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

    def get_date(self, year: int) -> date:
        return self.on.get_date(year, self.in_)


@dataclass
class Era:
    """
    Describes period during which a particular set of rules applies.
    NB. Referred to as a 'zonelet' in the C++ chrono library.
    """
    stdoff: TimeOffset
    rule: str
    format: str
    until: Until

    def __init__(self, fields: list[str]):
        self.stdoff = TimeOffset(fields.pop(0))
        self.rule = fields.pop(0)
        self.format = fields.pop(0)
        self.until = Until(fields)

    def get_tag(self, rule: Rule) -> str:
        fmt = self.format
        if '/' in fmt:
            return fmt.split('/')[1 if rule.save.is_dst else 0]
        return fmt.replace('%s', '' if rule.letters == '-' else rule.letters)

    def __str__(self):
        s = f'{self.stdoff} {self.rule} {self.format}'
        if self.until:
            s += f' {self.until}'
        return s

    def __repr__(self):
        s = f'{repr(self.stdoff)} {self.rule} {self.format}'
        if self.until:
            s += f' {repr(self.until)}'
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


@dataclass
class Zone(NamedItem):
    tzstr: str
    eras: list[Era]


@dataclass
class Link(NamedItem):
    zone: Zone


class TzData:
    def __init__(self):
        self.comments = []
        self.rules: dict[list[Rule]] = {}
        self.zones: list[Zone] = []
        self.links: list[Link] = []

    def add_rule(self, fields: list[str]) -> Rule:
        name = fields[1]
        rule = Rule(fields[2:])
        self.rules.setdefault(name, []).append(rule)
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
        era = Era(fields[2:])
        zone = Zone(name, tzstr, [era])
        self.zones.append(zone)
        return zone

    def add_link(self, fields: list[str]):
        tgt_name, link_name = fields[1:3]
        if link_name in self.links:
            print(f'{link_name} already specified, skipping')
            return
        zone = next(z for z in self.zones if z.name == tgt_name)
        link = Link(link_name, zone)
        self.links.append(link)

    def load(self, filename: str):
        zone = None
        # for line in open(TZDATA_PATH):
        for line in open(filename):
            line = line.strip()
            if not line:
                continue
            if line.startswith('#'):
                self.comments.append(line[1:].lstrip())
                continue
            line, _, _ = line.partition('#')
            fields = line.split()
            rectype = fields[0][0]
            if rectype == 'Z':
                zone = self.add_zone(fields)
            elif rectype == 'R':
                zone = None
                self.add_rule(fields)
            elif rectype == 'L':
                zone = None
                self.add_link(fields)
            elif zone:
                era = Era(fields)
                zone.eras.append(era)

    def load_compact(self, tzdata_path: str):
        """Load the single-file compact version of the database
        """
        self.load(os.path.join(tzdata_path, TZDATA_ZI))

    def load_full(self, tzdata_path: str):
        """Load the full version of the database
        Main advantage of this is that rule names are human-readable.
        """
        for name in TZDATA_FILE_LIST:
            self.load(os.path.join(tzdata_path, name))


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
    countries: list[Country] = None

    def load(self, tzdata: TzData, rootpath: str):
        # COUNTRIES
        countries = []
        for line in open(os.path.join(rootpath, COUNTRYTAB_FILENAME)):
            if line.startswith('#'):
                continue
            code, _, name = line.strip().partition('\t')
            countries.append(Country(code, name))
        def country_key(c):
            return c.sort_key
        self.countries = sorted(countries, key=country_key)

        # ZONES - naming as for `tzselect``
        self.timezones = []
        unique_regions = set()
        for line in open(os.path.join(rootpath, ZONETAB_FILENAME)):
            if line.startswith('#'):
                continue
            fields = line.strip().split('\t')
            country_codes = fields.pop(0).split(',')
            coordinates = fields.pop(0)
            tz = fields.pop(0)
            comments = fields.pop(0) if fields else ''
            try:
                zone = next(z for z in tzdata.zones if z.name == tz)
                self.timezones.append(TimeZone(country_codes, coordinates, zone, comments))
            except StopIteration:
                print(f'Zone {tz} not available')
                pass

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
            countries = [c for c in self.countries if c.code in codes]
            continent = Continent(continent_name, countries)
            self.continents.append(continent)
            for country in countries:
                # 3) Zone
                country.timezones = sorted([tz for tz in self.timezones if country.code in tz.country_codes])

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

