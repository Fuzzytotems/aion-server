#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/NpcData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * This is a container holding and serving all {@link NpcTemplate} instances.<br>
 * Briefly: Every {@link Npc} instance represents some class of NPCs among which each have the same id, name, items, statistics. Data for such NPC
 * class is defined in {@link NpcTemplate} and is uniquely identified by npc id.
 * <p>
 * C++: the @XmlTransient index points into the bound `npcs` storage, which stays after init (static-data.md §2.6). init (afterUnmarshal) is not
 * ported yet (P4-09: NpcStatCalculation of P5-01), so the index stays empty until then.
 *
 * @author Luno
 */
class NpcData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/NpcData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::npc::NpcTemplate*> npcData;

public:
	/** @return the npc template, nullptr (Java null) if there is none */
	const model::templates::npc::NpcTemplate* getNpcTemplate(int32_t id) const;
};

} // namespace aion::gameserver::dataholders
