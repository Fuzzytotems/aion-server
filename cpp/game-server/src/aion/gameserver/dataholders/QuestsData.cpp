#include "aion/gameserver/dataholders/QuestsData.h"

#include <string>

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestDrop.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/QuestService.h"

namespace aion::gameserver::dataholders {

using model::templates::QuestTemplate;

void QuestsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	questTemplates.clear();
	sortedByFactionId.clear();
	detail::JavaHashMapOrder<int32_t, const QuestTemplate*> order;
	for (const QuestTemplate& quest : questsData) {
		// Java: QuestEngine.init sets drop.setQuestId(data.getId()) on the published template (QuestEngine.java:89); C++: templates are const
		// once published, so the holder sets it here, before publication, and QuestEngine::init only reads it (docs/DEVIATIONS.md, "static data
		// templates", row QuestDrop.questId). The drops are elements of this holder's own storage, which is not const: the const_cast is well
		// defined.
		for (const model::templates::quest::QuestDrop& drop : quest.getQuestDrop())
			const_cast<model::templates::quest::QuestDrop&>(drop).setQuestId(quest.getId());
		questTemplates.insert_or_assign(quest.getId(), &quest);
		order.put(quest.getId(), &quest, detail::javaHashCode(quest.getId()));
		int32_t npcFactionId = quest.getNpcFactionId();
		if (npcFactionId == 0 || quest.isTimeBased())
			continue;
		sortedByFactionId[npcFactionId].push_back(&quest);
	}
	questsInHashOrder = order.values();
	// Java: questsData = null (the C++ index points into the storage, which stays)
}

const QuestTemplate* QuestsData::getQuestById(int32_t id) const {
	auto it = questTemplates.find(id);
	return it != questTemplates.end() ? it->second : nullptr;
}

std::vector<const QuestTemplate*> QuestsData::getQuestsByNpcFaction(int32_t npcFactionId, model::gameobjects::player::Player& player) const {
	auto factionQuests = sortedByFactionId.find(npcFactionId);
	if (factionQuests == sortedByFactionId.end())
		throw runtime::NullPointerException("Cannot invoke \"java.util.List.iterator()\" because \"factionQuests\" is null (npc faction " +
		                                    std::to_string(npcFactionId) + ")");
	std::vector<const QuestTemplate*> quests;
	for (const QuestTemplate* questTemplate : factionQuests->second) {
		if (!questEngine::QuestEngine::getInstance().isHaveHandler(questTemplate->getId()))
			continue;
		if (services::QuestService::checkStartConditions(player, questTemplate->getId(), false))
			quests.push_back(questTemplate);
	}
	return quests;
}

int32_t QuestsData::size() const {
	return static_cast<int32_t>(questTemplates.size());
}

std::vector<const QuestTemplate*> QuestsData::getQuestTemplates() const {
	return questsInHashOrder;
}

} // namespace aion::gameserver::dataholders
