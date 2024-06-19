COMPONENT_DEPENDS := \
	Timezone \
	SolarCalculator

HWCONFIG := basictz

DISABLE_NETWORK := 1

TZDATA_VERSION ?= latest

out/tzdb/version:
	$(TZDB_COMPILE_CMDLINE) @$(TZDATA_VERSION) $(@D)

COMPONENT_PREREQUISITES := out/tzdb/version

.PHONY: tzdata-clean
tzdata-clean:
	$(Q) rm -rf out/tzdb

clean: tzdata-clean

.PHONY: execute
execute: flash run
