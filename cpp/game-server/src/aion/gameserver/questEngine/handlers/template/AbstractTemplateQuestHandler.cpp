#include "aion/gameserver/questEngine/handlers/template/AbstractTemplateQuestHandler.h"

#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::questEngine::handlers::template_ {

AbstractTemplateQuestHandler::AbstractTemplateQuestHandler(int32_t questIdValue) : AbstractQuestHandler(questIdValue) {
}

bool AbstractTemplateQuestHandler::equalSets(const runtime::HashSet<int32_t>& a, const runtime::HashSet<int32_t>& b) {
	return a.size() == b.size() && a.containsAll(b.snapshot());
}

const gameserver::model::templates::QuestTemplate& AbstractTemplateQuestHandler::questTemplateOf(int32_t questId) {
	const gameserver::model::templates::QuestTemplate* template_ = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
	if (template_ == nullptr)
		throw runtime::NullPointerException("QUEST_DATA.getQuestById(" + std::to_string(questId) + ")");
	return *template_;
}

} // namespace aion::gameserver::questEngine::handlers::template_
