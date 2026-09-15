#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/NpcFactionsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.NpcFactionsData. @author vlog */
class NpcFactionsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/NpcFactionsData.xml.inc"
public:
	/** @return the faction template, nullptr (Java null) if there is none */
	const model::templates::factions::NpcFactionTemplate* getNpcFactionById(int32_t id) const;

	/** @return the faction template of the npc, nullptr (Java null) if there is none */
	const model::templates::factions::NpcFactionTemplate* getNpcFactionByNpcId(int32_t id) const;
};

} // namespace aion::gameserver::dataholders
