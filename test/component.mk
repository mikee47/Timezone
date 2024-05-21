COMPONENT_DEPENDS := \
	SmingTest \
	Timezone

COMPONENT_INCDIRS += out
COMPONENT_SRCDIRS += out

# Don't need network
HOST_NETWORK_OPTIONS := --nonet
DISABLE_NETWORK := 1

TZDATA=out/tzdata.h out/tzdata.cpp

$(TZDATA):
	$(TZ_COMPILE_CMDLINE) --name --tzstr --transitions --from 2020 --to 2040 full out

COMPONENT_PREREQUISITES := $(TZDATA)

.PHONY: tzdata-clean
tzdata-clean:
	$(Q) rm -f $(TZDATA)

clean: tzdata-clean

.PHONY: execute
execute: flash run
