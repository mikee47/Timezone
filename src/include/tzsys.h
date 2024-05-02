#pragma once

#include <WString.h>
#include <time.h>

#ifdef ARCH_HOST

#include "../Arch/Host/tzset.h"

#else

extern "C" {
#if __NEWLIB__ >= 4
// Previous versions declared in time.h
#include <sys/_tz_structs.h>
#endif
}

#endif

namespace TZ
{
#ifndef ARCH_HOST

extern "C" int __tzcalc_limits(int);

static inline int calcLimits(int year)
{
	return __tzcalc_limits(year);
}

static inline __tzinfo_type& getInfo()
{
	return *__gettzinfo();
}

static inline void setZone(const char* tzenv)
{
	setenv("TZ", tzenv, 1);
	::tzset();
}
#endif

static inline void setZone(const String& tzenv)
{
	return setZone(tzenv.c_str());
}

} // namespace TZ
