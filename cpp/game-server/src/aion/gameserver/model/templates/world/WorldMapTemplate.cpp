#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"

#include <algorithm>
#include <atomic>

#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/world/zone/ZoneAttributesInfo.h"

namespace aion::gameserver::model::templates::world {

using ::aion::gameserver::world::zone::ZoneAttributes;

// Java WorldMapTemplate.java:95-100
int32_t WorldMapTemplate::getTwinCount() const {
	int32_t channelCount = std::max(1, twinCount);
	int32_t maxTwins = configs::main::WorldConfig::WORLD_MAX_TWINS_USUAL.load(std::memory_order_relaxed);
	if (maxTwins == 0)
		return channelCount;
	return std::min(maxTwins, channelCount);
}

bool WorldMapTemplate::isFly() const {
	return hasAttribute(ZoneAttributes::FLY);
}

bool WorldMapTemplate::canGlide() const {
	return hasAttribute(ZoneAttributes::GLIDE);
}

bool WorldMapTemplate::canPutKisk() const {
	return hasAttribute(ZoneAttributes::BIND);
}

bool WorldMapTemplate::canRecall() const {
	return hasAttribute(ZoneAttributes::RECALL);
}

bool WorldMapTemplate::canRide() const {
	return hasAttribute(ZoneAttributes::RIDE);
}

bool WorldMapTemplate::canFlyRide() const {
	return hasAttribute(ZoneAttributes::FLY_RIDE);
}

bool WorldMapTemplate::isPvpAllowed() const {
	return hasAttribute(ZoneAttributes::PVP_ENABLED);
}

bool WorldMapTemplate::isSameRaceDuelsAllowed() const {
	return hasAttribute(ZoneAttributes::DUEL_SAME_RACE_ENABLED);
}

bool WorldMapTemplate::isOtherRaceDuelsAllowed() const {
	return hasAttribute(ZoneAttributes::DUEL_OTHER_RACE_ENABLED);
}

bool WorldMapTemplate::canReturnToBattle() const {
	return hasAttribute(ZoneAttributes::NO_RETURN_BATTLE);
}

int32_t WorldMapTemplate::getFlags() const {
	int32_t result = 0;
	for (ZoneAttributes attribute : zoneAttributes)
		result |= ::aion::gameserver::world::zone::getId(attribute);
	return result;
}

} // namespace aion::gameserver::model::templates::world
