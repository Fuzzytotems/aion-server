#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/dataholders/NpcData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * This is a container holding and serving all {@link NpcTemplate} instances.<br>
 * Briefly: Every {@link Npc} instance represents some class of NPCs among which each have the same id, name, items, statistics. Data for such NPC
 * class is defined in {@link NpcTemplate} and is uniquely identified by npc id.
 * <p>
 * C++: the @XmlTransient index points into the bound `npcs` storage, which stays after init (static-data.md §2.6). init runs inline through
 * LoadContext::runAfterUnmarshalTask (Java may run it asynchronously), so size() is final when StaticData logs it. init fills missing stats
 * into the npc templates before the holder is published. getNpcData returns the templates in Java's HashMap<Integer, NpcTemplate> order.
 *
 * @author Luno
 */
class NpcData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/NpcData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::npc::NpcTemplate*> npcData;
	std::unordered_set<int32_t> functionDialogIds;
	/** C++ only: npcData.values() in Java's HashMap iteration order */
	std::vector<const model::templates::npc::NpcTemplate*> npcsInHashOrder;

	void init();

public:
	int32_t size() const;

	/** @return the npc template, nullptr (Java null) if there is none */
	const model::templates::npc::NpcTemplate* getNpcTemplate(int32_t id) const;

	/** Java `npcData.values()` (HashMap iteration order) */
	const std::vector<const model::templates::npc::NpcTemplate*>& getNpcData() const;

	bool isFunctionDialog(int32_t functionDialogId) const;
};

} // namespace aion::gameserver::dataholders
