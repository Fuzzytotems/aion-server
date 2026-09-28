#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/ChestData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.ChestData.
 * <p>
 * C++: the index points into the bound `chests` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author Wakizashi
 */
class ChestData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ChestData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::chest::ChestTemplate*> chestData;

public:
	int32_t size() const;

	/** @return the chest template of the npc, nullptr (Java null) if there is none */
	const model::templates::chest::ChestTemplate* getChestTemplate(int32_t npcId) const;
};

} // namespace aion::gameserver::dataholders
