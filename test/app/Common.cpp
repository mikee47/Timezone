#include <Timezone.h>

String toString(const ZonedTime& time)
{
	return time.format(_F("%a, %d %b %Y %T %Z"));
}
