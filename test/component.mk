COMPONENT_DEPENDS := \
	SmingTest \
	Timezone

# Don't need network
HOST_NETWORK_OPTIONS := --nonet
DISABLE_NETWORK := 1

# Include transition data for verification
APP_TZDATA_OPTS := --name --tzstr --transitions --from 2020 --to 2040 full

.PHONY: execute
execute: flash run
