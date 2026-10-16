#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/QuestsData.xml.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.QuestsData.
 * <p>
 * C++: the @XmlTransient index maps point into the bound `questsData` storage, which stays after afterUnmarshal (static-data.md §2.6; Java
 * sets the list to null). getQuestTemplates returns the quests in Java's HashMap<Integer, QuestTemplate> iteration order, computed by
 * afterUnmarshal (detail::JavaHashMapOrder).
 *
 * @author MrPoke
 */
class QuestsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/QuestsData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::QuestTemplate*> questTemplates;
	std::unordered_map<int32_t, std::vector<const model::templates::QuestTemplate*>> sortedByFactionId;
	/** C++ only: questTemplates.values() in Java's HashMap iteration order */
	std::vector<const model::templates::QuestTemplate*> questsInHashOrder;

public:
	/** @return the quest template, nullptr (Java null) if there is none */
	const model::templates::QuestTemplate* getQuestById(int32_t id) const;

	/** @throws NullPointerException if the faction has no quests (Java iterates the null list) */
	std::vector<const model::templates::QuestTemplate*> getQuestsByNpcFaction(int32_t npcFactionId, model::gameobjects::player::Player& player) const;

	int32_t size() const;

	/** Java `questTemplates.values()` (HashMap iteration order) */
	std::vector<const model::templates::QuestTemplate*> getQuestTemplates() const;
};

} // namespace aion::gameserver::dataholders
