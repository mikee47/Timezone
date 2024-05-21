#include "Common.h"
#include <Data/CStringArray.h>

namespace
{
#define TEST_ZONE_NAME_MAP(XX)                                                                                         \
	XX("europelondon", &TZ::Europe::London::info)                                                                      \
	XX("africa/porto   novo", &TZ::Africa::Porto_Novo::info)                                                           \
	XX("america/boavista", &TZ::America::Boa_Vista::info)                                                              \
	XX("pacific chatham", &TZ::Pacific::Chatham::info)                                                                 \
	XX("Pacific Chatham2", nullptr)

struct TestName {
	PGM_P name;
	const TZ::Info* info;
};

#define XX(name, info) {name, info},
const TestName testNames[] PROGMEM{TEST_ZONE_NAME_MAP(XX)};
#undef XX

DEFINE_FSTR(badPosixStrings, "\0"
							 "G\0"
							 "GM\0"
							 "GMT\0"
							 "GMT0x\0"
							 "GMT0DST,\0")

} // namespace

class BasicTest : public TestGroup
{
public:
	BasicTest() : TestGroup(_F("Basic"))
	{
	}

	void execute() override
	{
		TEST_CASE("String parsing")
		{
			Timezone tz;
			tz = Timezone::fromPosix("GMT0");
			CHECK(tz);

			for(auto s : CStringArray(badPosixStrings)) {
				tz = Timezone::fromPosix(s);
				CHECK(!tz);
			}
		}

		TEST_CASE("Find zone")
		{
			for(auto& t : testNames) {
				auto zone = TZ::findZone(t.name);
				if(zone) {
					Serial << _F("Matched '") << t.name << _F("' to ") << zone->location << endl;
				}
				CHECK(zone == t.info);
			}
		}
	}
};

void REGISTER_TEST(basic)
{
	registerGroup<BasicTest>();
}
