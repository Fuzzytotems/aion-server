#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"

#include <algorithm>
#include <atomic>

#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"

namespace aion::gameserver::model::templates::world {

namespace {

using ::aion::gameserver::world::zone::ZoneAttributes;

/**
 * Java ZoneAttributes.getId(): the constructor argument, which is `1 << ordinal` for every constant (BIND 1 << 0 ... NO_RETURN_BATTLE 1 << 9).
 * The companion of ZoneAttributes belongs to P4-10 and does not exist yet.
 */
constexpr int32_t zoneAttributeId(ZoneAttributes attribute) noexcept {
	return 1 << static_cast<int32_t>(attribute);
}

} // namespace

void WorldMapTemplate::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	// Java: flags = ZoneAttributes.fromList(flagValues), which throws NullPointerException without the flags attribute.
	// Deviation: StaticDataException with the location instead (every map of the data has the attribute; docs/deviations/P4-07a.md)
	if (!flagValues.has_value())
		ctx.fail("the flags attribute is missing (Java: NullPointerException in ZoneAttributes.fromList)");
	int32_t result = 0;
	for (size_t ordinal = 0; ordinal < xml::EnumTraits<ZoneAttributes>::names.size(); ++ordinal) {
		ZoneAttributes attribute = static_cast<ZoneAttributes>(ordinal);
		if (std::ranges::find(*flagValues, attribute) != flagValues->end())
			result |= zoneAttributeId(attribute);
	}
	flags = result;
}

int32_t WorldMapTemplate::getTwinCount() const {
	int32_t maxTwins = configs::main::WorldConfig::WORLD_MAX_TWINS_USUAL.load(std::memory_order_relaxed);
	if (maxTwins == 0)
		return twinCount;
	return std::min(maxTwins, twinCount);
}

int32_t WorldMapTemplate::getBeginnerTwinCount() const {
	int32_t maxTwins = configs::main::WorldConfig::WORLD_MAX_TWINS_BEGINNER.load(std::memory_order_relaxed);
	if (maxTwins == 0)
		return beginnerTwinCount;
	else if (maxTwins == -1) // disabled
		return 0;
	return std::min(maxTwins, beginnerTwinCount);
}

bool WorldMapTemplate::isFly() const {
	return (flags & zoneAttributeId(ZoneAttributes::FLY)) != 0;
}

bool WorldMapTemplate::canGlide() const {
	return (flags & zoneAttributeId(ZoneAttributes::GLIDE)) != 0;
}

bool WorldMapTemplate::canPutKisk() const {
	return (flags & zoneAttributeId(ZoneAttributes::BIND)) != 0;
}

bool WorldMapTemplate::canRecall() const {
	return (flags & zoneAttributeId(ZoneAttributes::RECALL)) != 0;
}

bool WorldMapTemplate::canRide() const {
	return (flags & zoneAttributeId(ZoneAttributes::RIDE)) != 0;
}

bool WorldMapTemplate::canFlyRide() const {
	return (flags & zoneAttributeId(ZoneAttributes::FLY_RIDE)) != 0;
}

bool WorldMapTemplate::isPvpAllowed() const {
	return (flags & zoneAttributeId(ZoneAttributes::PVP_ENABLED)) != 0;
}

bool WorldMapTemplate::isSameRaceDuelsAllowed() const {
	return (flags & zoneAttributeId(ZoneAttributes::DUEL_SAME_RACE_ENABLED)) != 0;
}

bool WorldMapTemplate::isOtherRaceDuelsAllowed() const {
	return (flags & zoneAttributeId(ZoneAttributes::DUEL_OTHER_RACE_ENABLED)) != 0;
}

bool WorldMapTemplate::canReturnToBattle() const {
	return (flags & zoneAttributeId(ZoneAttributes::NO_RETURN_BATTLE)) != 0;
}

} // namespace aion::gameserver::model::templates::world
