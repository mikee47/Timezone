#define TZINFO_WANT_TZSTR 1

#include "include/tzdb.h"

namespace
{
bool matchLocation(const String& location, const char* name)
{
	auto loc = location.c_str();
	while(*name && *loc) {
		if(!isalpha(*name)) {
			++name;
			continue;
		}
		if(!isalpha(*loc)) {
			++loc;
			continue;
		}
		if(tolower(*name) != tolower(*loc)) {
			return false;
		}
		++name;
		++loc;
	}

	return *name == '\0' && *loc == '\0';
}

} // namespace

namespace TZ
{
DEFINE_FSTR_LOCAL(fstr_empty, "")
DEFINE_REF_LOCAL(area, fstr_empty)
TIMEZONE_BEGIN(Empty, "", "")
TZ_DEFINE_PSTR_LOCAL(tzstr, nullptr)
DEFINE_REF_LOCAL(dst_rule, rule_none)
DEFINE_REF_LOCAL(std_rule, rule_none)
DEFINE_REF_LOCAL(transitions, transitions_none)
TIMEZONE_END()

const Info& Info::empty()
{
	return Empty::info;
}

const Info* findZone(const String& name)
{
	auto namelen = name.length();
	for(auto areaPair : areas) {
		auto& area = areaPair.key();
		auto arealen = area.length();
		if(namelen < arealen) {
			continue;
		}
		auto nameptr = name.c_str();
		if(!area.equalsIgnoreCase(nameptr, arealen)) {
			continue;
		}
		nameptr += arealen;
		for(auto& zone : areaPair.content()) {
			if(matchLocation(zone.location, nameptr)) {
				return &zone;
			}
		}
	}

	return nullptr;
}

} // namespace TZ
