#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/NpcFactionsData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.NpcFactionsData.
 * <p>
 * C++: the indexes point into the bound `npcFactionsData` storage, which stays after afterUnmarshal (static-data.md §2.6).
 * getNpcFactionsData returns the factions in Java's HashMap<Integer, NpcFactionTemplate> iteration order.
 *
 * @author vlog
 */
class NpcFactionsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/NpcFactionsData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::factions::NpcFactionTemplate*> factionsById;
	std::unordered_map<int32_t, const model::templates::factions::NpcFactionTemplate*> factionsByNpcId;
	/** C++ only: factionsById.values() in Java's HashMap iteration order */
	std::vector<const model::templates::factions::NpcFactionTemplate*> factionsInHashOrder;

public:
	/** @return the faction template, nullptr (Java null) if there is none */
	const model::templates::factions::NpcFactionTemplate* getNpcFactionById(int32_t id) const;

	/** @return the faction template of the npc, nullptr (Java null) if there is none */
	const model::templates::factions::NpcFactionTemplate* getNpcFactionByNpcId(int32_t id) const;

	const std::vector<const model::templates::factions::NpcFactionTemplate*>& getNpcFactionsData() const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
