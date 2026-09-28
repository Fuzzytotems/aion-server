#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/CustomDrop.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.CustomDrop.
 * <p>
 * C++: the index points into the bound `npcDrop` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author ViAl, Neon
 */
class CustomDrop : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/CustomDrop.xml.inc"
private:
	std::unordered_map<int32_t, const model::drop::NpcDrop*> dropById;

public:
	/** @return the custom drop of the npc, nullptr (Java null) if there is none */
	const model::drop::NpcDrop* getNpcDrop(int32_t npcId) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
