/****
 * tzdb.h - Time Zone database support
 *
 * This file is part of the Timezone Library
 *
 * This library is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, version 3 or later.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this library.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 ****/

#pragma once

#include "Timezone.h"
#include <FlashString/Array.hpp>
#include <FlashString/Vector.hpp>
#include <FlashString/Map.hpp>

#if TZINFO_WANT_NAME
#define TZINFO_FIELD_NAME area,
#else
#define TZINFO_FIELD_NAME
#endif

#if TZINFO_WANT_TZSTR
#define TZINFO_FIELD_TZSTR tzstr,
#else
#define TZINFO_FIELD_TZSTR
#endif

#if TZINFO_WANT_RULES
#define TZINFO_FIELD_RULES dst_rule, std_rule,
#else
#define TZINFO_FIELD_RULES
#endif

#if TZINFO_WANT_TRANSITIONS
#define TZINFO_FIELD_TRANSITIONS tznames, transitions,
#else
#define TZINFO_FIELD_TRANSITIONS
#endif

// clang-format off
#define DEFINE_REF_LOCAL(name, refname) \
	static constexpr decltype(refname)& name = refname;

#define TZ_DEFINE_RULE_LOCAL(name, ...) \
	static constexpr const Rule name PROGMEM {__VA_ARGS__};

#define TZ_DEFINE_PSTR_LOCAL(name, s) \
    static constexpr const char* name PROGMEM_PSTR  = s;

#define TIMEZONE_BEGIN(clsname, area_, location_) \
	class clsname : public Timezone { \
	public: \
		clsname() : Timezone(fromPosix(tzstr)) { } \
		TZ_DEFINE_PSTR_LOCAL(location, location_)

#define TIMEZONE_END() \
	static constexpr const Info info PROGMEM { \
		location, \
		TZINFO_FIELD_NAME \
		TZINFO_FIELD_TZSTR \
		TZINFO_FIELD_RULES \
		TZINFO_FIELD_TRANSITIONS \
		}; \
	};
// clang-format on

namespace TZ
{
struct Transition {
	int64_t time : 40;
	uint8_t desigidx : 8;
	int16_t offsetMins : 15;
	bool isdst : 1;

	operator time_t() const
	{
		return time;
	}

	time_t local() const
	{
		return time + (offsetMins * 60);
	}
};

#ifndef __WIN32
static_assert(sizeof(Transition) == 8, "Bad Transition def");
#endif

static constexpr const Rule PROGMEM rule_none{};
DEFINE_FSTR_ARRAY_LOCAL(transitions_none, Transition)

struct Info {
	PGM_P location;
#if TZINFO_WANT_NAME
	const FSTR::String& area;
#endif
#if TZINFO_WANT_TZSTR
	PGM_P tzstr;
#endif
#if TZINFO_WANT_RULE
	const Rule& dstStart;
	const Rule& stdStart;
#endif
#if TZINFO_WANT_TRANSITIONS
	PGM_P tznames;
	const FSTR::Array<Transition>& transitions;
#endif

#if TZINFO_WANT_NAME
	String name() const
	{
		String s;
		s += area;
		s += '/';
		s += location;
		return s;
	}
#endif

	operator Timezone() const
	{
#if TZINFO_WANT_TZSTR
		return Timezone::fromPosix(tzstr);
#elif TZINFO_WANT_RULE
		return Timezone(dstStart, stdStart);
#endif
	}

#if TZINFO_WANT_TRANSITIONS
	DateTime::ZoneInfo getInfo(const Transition& tt) const
	{
		return DateTime::ZoneInfo{
			.tag = Rule::Tag::fromString(&tznames[tt.desigidx]),
			.offsetMins = tt.offsetMins,
			.isDst = tt.isdst,
		};
	}
#endif

	explicit operator bool() const
	{
		return location;
	}

	static const Info& empty();
};

using ZoneList = FSTR::Vector<Info>;
using AreaMap = FSTR::Map<FSTR::String, ZoneList>;

DECLARE_FSTR_MAP(areas, FSTR::String, ZoneList)

/**
 * @brief Find a zone given its full name
 * @param name Full name of zone
 * @retval Info* Zone information, if found
 * Comparison is performed on full name without case-sensitivity and with all punctuation removed.
 * Thus:
 * 	"europe-london" matches "Europe/London"
 *  "africa/porto_novo" matches "Africa/Porto-Novo"
 *	"america port au  prince" matches "America/Port-au-Prince"
 *
 * This makes things a bit easier with little risk of false-positives.
 * 
 */
const Info* findZone(const String& name);

} // namespace TZ
