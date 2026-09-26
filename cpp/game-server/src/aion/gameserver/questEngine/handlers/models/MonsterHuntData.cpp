#include "aion/gameserver/questEngine/handlers/models/MonsterHuntData.h"

#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestKill.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::questEngine::handlers::models {

void MonsterHuntData::register_(QuestEngine& /*questEngine*/) const {
	AION_UNPORTED();
}

std::optional<std::unordered_set<int32_t>> MonsterHuntData::getAlternativeNpcs(int32_t npcId) const {
	if (auto others = otherNpcIds(startNpcIds, npcId))
		return others;
	if (auto others = otherNpcIds(endNpcIds, npcId))
		return others;
	if (auto others = otherNpcIds(aggroNpcIds, npcId))
		return others;
	const gameserver::model::templates::QuestTemplate* questTemplate = dataholders::DataManager::QUEST_DATA->getQuestById(id);
	if (questTemplate == nullptr) // Java: NullPointerException on getQuestKill()
		throw runtime::NullPointerException("Quest template " + std::to_string(id) + " does not exist");
	for (const gameserver::model::templates::quest::QuestKill& qk : questTemplate->getQuestKill()) {
		if (auto others = otherNpcIds(qk.getNpcIds(), npcId))
			return others;
	}
	return std::nullopt;
}

} // namespace aion::gameserver::questEngine::handlers::models
