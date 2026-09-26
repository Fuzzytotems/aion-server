#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.xml.h"

namespace aion::gameserver::model::templates::spawns {

/** Java com.aionemu.gameserver.model.templates.spawns.SpawnSpotTemplate. @author xTz, Rolandas */
class SpawnSpotTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.xml.inc"
public:
	SpawnSpotTemplate() = default;

	/**
	 * Java `SpawnSpotTemplate(float x, float y, float z, byte h, int randomWalk, String walkerId, Integer walkerIndex)` (SpawnsData custom spawns
	 * and search results). A null walkerId is the empty string of the member.
	 */
	SpawnSpotTemplate(float x, float y, float z, int8_t h, int32_t randomWalk, std::optional<std::string_view> walkerId,
		std::optional<int32_t> walkerIndex);

	int32_t getStaticId() const { return staticId; }

	/** mutates spawn data (a MutableHolderRef); the Java sources never call it on a spot */
	void setStaticId(int32_t value) { staticId = value; }

	int32_t getRandomWalk() const { return randomWalk; }

	int32_t getState() const { return state; }

	bool isAerialSpawn() const { return aerialSpawn.value_or(false); }
};

} // namespace aion::gameserver::model::templates::spawns
