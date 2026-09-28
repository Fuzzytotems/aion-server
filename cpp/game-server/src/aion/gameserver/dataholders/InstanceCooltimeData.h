#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "aion/gameserver/dataholders/InstanceCooltimeData.xml.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.InstanceCooltimeData.
 * <p>
 * C++: the indexes point into the bound `instanceCooltime` storage, which stays (Java clears the list). `instanceCooltimes` is Java's
 * LinkedHashMap: an index map plus the world ids in insertion order (a repeated world id keeps its first position, as put does).
 *
 * @author VladimirZ
 */
class InstanceCooltimeData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/InstanceCooltimeData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::InstanceCooltime*> instanceCooltimes;
	/** C++ only: the keys of instanceCooltimes in insertion order (Java LinkedHashMap) */
	std::vector<int32_t> worldIdsInOrder;
	std::unordered_map<int32_t, int32_t> syncIdToMapId;

public:
	/** Java: a copy of the LinkedHashMap, as (world id, cooltime) pairs in insertion order */
	std::vector<std::pair<int32_t, const model::templates::InstanceCooltime*>> getInstanceCooltimes() const;

	/** @return the cooltime template of the world, nullptr (Java null) if there is none */
	const model::templates::InstanceCooltime* getInstanceCooltimeByWorldId(int32_t worldId) const;

	/** Java: NullPointerException if the world has no cooltime template */
	int32_t getInstanceMaxCountByWorldId(int32_t worldId) const;

	int32_t getMaxMemberCount(int32_t worldId, model::Race race) const;

	int32_t getWorldId(int32_t syncId) const;

	int64_t calculateInstanceEntranceCooltime(model::gameobjects::player::Player& player, int32_t worldId) const;

private:
	static int32_t calculateDaysUntilReset(const model::templates::InstanceCooltime& clt, int32_t dayOfWeek);

	static int32_t getDay(std::string_view day);

public:
	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
