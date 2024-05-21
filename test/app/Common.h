#include <SmingTest.h>
#include <Timezone.h>
#include <tzdata.h>

String toString(const ZonedTime& time);

inline String toString(time_t utc)
{
	return toString(ZonedTime(utc));
}
