#include "aion/gameserver/controllers/effect/CumulativeResist.h"

#include "aion/gameserver/controllers/effect/CumulativeResistType.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::controllers::effect {

namespace {

/**
 * Java System.currentTimeMillis(): the clock of the installed scheduler backend - the system clock in the server, the ManualClock under a
 * DeterministicExecutor - the same time line as Effect's end time (docs/deviations/P5-02b.md)
 */
int64_t currentTimeMillis() {
	return utils::ThreadPoolManager::clock().currentTimeMillis();
}

} // namespace

CumulativeResist::CumulativeResist() = default;

CumulativeResist::~CumulativeResist() = default;

runtime::Ref<CumulativeResist> CumulativeResist::create() {
	return runtime::makeRef<CumulativeResist>();
}

// Java: package-private and unsynchronized; every caller holds PlayerEffectController's `synchronized (cumulativeResistInfo)`
void CumulativeResist::tryIncrementLevel(int64_t maxDurationMillis) {
	resetIfExpired();
	if (level.get() < 5)
		level.set(level.get() + 1);
	expirationTime.set(currentTimeMillis() + maxDurationMillis);
}

float CumulativeResist::getDurationMultiplier() {
	// time_value* from repeated_abnormal_status_immune.xml retail file
	switch (level.get()) {
		case 0:
		case 1:
			return 1;
		case 2:
			return 0.9f;
		case 3:
			return 0.85f;
		case 4:
			return 0.8f;
		default:
			return 0;
	}
}

int32_t CumulativeResist::getCooldownTimeOffset(CumulativeResistType type) {
	// holding_time2 from repeated_abnormal_status_immune.xml retail file
	switch (type) {
		case CumulativeResistType::SLEEP:
		case CumulativeResistType::PARALYZE:
			return 0;
		case CumulativeResistType::FEAR:
			return 2000;
	}
	return 0; // Java: the switch expression over the enum is exhaustive
}

int32_t CumulativeResist::getResistance() {
	resetIfExpired();
	// resist_value* from repeated_abnormal_status_immune.xml retail file
	switch (level.get()) {
		case 0:
		case 1:
		case 2:
			return 0;
		case 3:
			return 200;
		case 4:
			return 400;
		default:
			return 1000;
	}
}

void CumulativeResist::resetIfExpired() {
	if (level.get() > 0 && currentTimeMillis() > expirationTime.get())
		level.set(0);
}

} // namespace aion::gameserver::controllers::effect
