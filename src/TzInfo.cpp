#include "include/Timezone/TzInfo.h"
#include <Data/Stream/FileStream.h>

void ZoneData::normalize(String& name)
{
	bool up{true};
	for(auto& c : name) {
		if(c == '/' || c == '_') {
			up = true;
		} else if(c == ' ') {
			c = '_';
			up = true;
		} else if(up) {
			c = toupper(c);
			up = false;
		} else {
			c = tolower(c);
		}
	}
}

String ZoneData::findZone(const String& name, bool includeLinks)
{
	String normalisedName = normalize(name);
	String area;
	String location;
	int i = normalisedName.indexOf('/');
	if(i < 0) {
		area = F("default");
		location = normalisedName;
	} else {
		area = normalisedName.substring(0, i);
		location = normalisedName.substring(i + 1);
	}

	if(area != currentArea) {
		zoneTable = std::make_unique<ZoneTable>(new FileStream(area + ".zi"), ' ', "", 256);
		currentArea = area;
	}

	if(!zoneTable) {
		currentArea = nullptr;
		return nullptr;
	}

	for(auto rec : *zoneTable) {
		switch(rec.type()) {
		case TzInfoRecord::Type::zone: {
			ZoneRecord zone(rec);
			if(location == zone.location()) {
				scanZone();
				return normalisedName;
			}
			break;
		}

		case TzInfoRecord::Type::link: {
			if(!includeLinks) {
				break;
			}
			LinkRecord link(rec);
			if(location == link.location()) {
				return findZone(link.zone(), false);
			}
			break;
		}

		case TzInfoRecord::Type::invalid:
		case TzInfoRecord::Type::era:
			break;
		}
	}

	return nullptr;
}

void ZoneData::scanZone()
{
	auto cursor = zoneTable->tell();
	unsigned numEras{0};
	while(auto rec = zoneTable->next()) {
		if(rec.type() != TzInfoRecord::Type::era) {
			break;
		}
		++numEras;
	}

	timezone.eras = std::make_unique<TzData::Era[]>(numEras);
	timezone.numEras = numEras;
	assert(zoneTable->seek(cursor));
	for(unsigned i = 0; i < numEras; ++i) {
		auto& era = timezone.eras[i];
		auto rec = zoneTable->next();
		EraRecord er(rec);

		era.stdoff = er.stdoff();
		era.until = er.until();
		era.format = getStrPtr(er.format());
		auto rule = er.rule();
		if(rule && *rule != '-') {
			era.rule = loadRule(rule);
		} else {
			era.dstoff = TzData::TimeOffset(rule);
		}
	}
}

TzData::StrPtr ZoneData::getStrPtr(const char* str)
{
	if(!str) {
		str = "";
	}
	int i = strings.indexOf(str);
	if(i < 0) {
		i = strings.count();
		strings.add(str);
	}
	return unsigned(i);
}

TzData::Rule* ZoneData::loadRule(const char* name)
{
	int i = rules.indexOf(name);
	if(i >= 0) {
		return &rules[i];
	}

	auto table = std::make_unique<CsvTable<RuleRecord>>(new FileStream(F("rules/") + name), ' ', "", 64);
	uint16_t count{0};
	for(auto rec : *table) {
		++count;
	}

	auto rule = new TzData::Rule(name, count);
	table->reset();
	for(unsigned i = 0; i < count; ++i) {
		auto rec = table->next();
		RuleRecord r(rec);
		rule->lines[i] = {r.from(), r.to(), r.in(), r.on(), r.at(), r.save(), getStrPtr(r.letters())};
	}

	rules.addElement(rule);

	return rule;
}