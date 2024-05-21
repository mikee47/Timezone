#include "Common.h"

class TransitionsTest : public TestGroup
{
public:
	TransitionsTest() : TestGroup(_F("Transitions"))
	{
	}

	void execute() override
	{
#if TZINFO_WANT_TRANSITIONS
		for(auto area : TZ::areas) {
			for(auto& zone : area.content()) {
				if(zone.transitions.length() == 0) {
					continue;
				}

				Timezone tz(zone);

				unsigned mismatch{0};
				ZonedTime t;
				ZonedTime tPrev;
				auto tt = zone.transitions.begin();
				auto ttPrev = *tt++;
				for(; tt != zone.transitions.end(); tPrev = t, ttPrev = *tt, ++tt) {
					auto ytt = DateTime(*tt).Year;
					t = tPrev;
					while(DateTime(t).Year < ytt || t.isDst() != (*tt).isdst) {
						tPrev = t;
						t = tz.getNextChange(tPrev);
					}
					if(t == *tt) {
						continue;
					}

					if(mismatch++ == 0) {
						Serial << endl
							   << zone.name()
#if TZINFO_WANT_TZSTR
							   << ": " << zone.tzstr
#endif
							   << " " << tz << endl
							   << String().pad(12) << F("Calculated").pad(30) << _F("\tFrom transition table") << endl;
					}

					const char* tags[] = {"STD", "DST"};
					ZonedTime trFrom(*tt, zone.getInfo(ttPrev));
					ZonedTime trTo(*tt, zone.getInfo(*tt));
					ZonedTime tFrom = tz.makeZoned(t, true);
					Serial << (F("- From ") + tags[tFrom.isDst()]).pad(12) << toString(tFrom) << '\t'
						   << toString(trFrom) << endl;
					Serial << (F("  To ") + tags[t.isDst()]).pad(12) << toString(t) << '\t' << toString(trTo) << endl;
					Serial << String().pad(12) << toString(t.toUtc()) << '\t' << toString(*tt) << endl;
				}
			}
		}
#endif // TZINFO_WANT_TRANSITIONS
	}
};

void REGISTER_TEST(transitions)
{
	registerGroup<TransitionsTest>();
}
