#include "aion/gameserver/controllers/effect/CumulativeResist.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::controllers::effect {

CumulativeResist::CumulativeResist() = default;

CumulativeResist::~CumulativeResist() = default;

runtime::Ref<CumulativeResist> CumulativeResist::create() {
	return runtime::makeRef<CumulativeResist>();
}

void CumulativeResist::tryIncrementLevel(int64_t maxDurationMillis) {
	AION_UNPORTED();
}

float CumulativeResist::getDurationMultiplier() {
	AION_UNPORTED();
}

int32_t CumulativeResist::getCooldownTimeOffset(CumulativeResistType type) {
	AION_UNPORTED();
}

int32_t CumulativeResist::getResistance() {
	AION_UNPORTED();
}

void CumulativeResist::resetIfExpired() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::effect
