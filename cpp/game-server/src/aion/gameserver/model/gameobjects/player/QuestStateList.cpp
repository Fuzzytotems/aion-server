#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestCategory.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"

namespace aion::gameserver::model::gameobjects::player {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.gameobjects.player.QuestStateList");

QuestStateList::QuestStateList() = default;

QuestStateList::~QuestStateList() = default;

runtime::Ref<QuestStateList> QuestStateList::create() {
	return runtime::makeRef<QuestStateList>();
}

bool QuestStateList::hasQuest(int32_t questId) {
	return quests.containsKey(questId);
}

bool QuestStateList::addQuest(int32_t questId, questEngine::model::QuestState& questState) {
	SYNCHRONIZED(*this) {
		if (quests.containsKey(questId)) {
			log.warn("Tried to add duplicate quest to quest list: " + std::to_string(questId));
			return false;
		}
		quests.put(questId, runtime::Ref<questEngine::model::QuestState>(questState));
		return true;
	}
}

runtime::Ptr<questEngine::model::QuestState> QuestStateList::deleteQuest(int32_t questId) {
	SYNCHRONIZED(*this) {
		runtime::Ptr<questEngine::model::QuestState> qs = quests.remove(questId);
		if (qs) {
			deletedQuests.add(qs->getQuestId());
			qs->setPersistentState(Persistable::PersistentState::DELETED);
		}
		return qs;
	}
}

runtime::Ptr<questEngine::model::QuestState> QuestStateList::getQuestState(int32_t questId) {
	return quests.get(questId);
}

std::vector<runtime::Ptr<questEngine::model::QuestState>> QuestStateList::getAllQuestState() {
	return quests.values();
}

std::vector<runtime::Ptr<questEngine::model::QuestState>> QuestStateList::getCompletedQuests() {
	std::vector<runtime::Ptr<questEngine::model::QuestState>> result;
	for (const runtime::Ptr<questEngine::model::QuestState>& qs : quests.values()) {
		if (qs->getCompleteCount() > 0)
			result.push_back(qs);
	}
	return result;
}

std::vector<runtime::Ptr<questEngine::model::QuestState>> QuestStateList::getUncompletedQuests() {
	std::vector<runtime::Ptr<questEngine::model::QuestState>> result;
	for (const runtime::Ptr<questEngine::model::QuestState>& qs : quests.values()) {
		if (qs->getStatus() != questEngine::model::QuestStatus::COMPLETE)
			result.push_back(qs);
	}
	return result;
}

std::vector<runtime::Ptr<questEngine::model::QuestState>> QuestStateList::getNormalQuests() {
	std::vector<runtime::Ptr<questEngine::model::QuestState>> questList;
	for (const runtime::Ptr<questEngine::model::QuestState>& qs : getAllQuestState()) {
		const model::templates::QuestTemplate* questTemplate = dataholders::DataManager::QUEST_DATA->getQuestById(qs->getQuestId());
		if (questTemplate == nullptr) // Java: NullPointerException on getCategory()
			throw runtime::NullPointerException("QuestTemplate of quest " + std::to_string(qs->getQuestId()));
		model::templates::quest::QuestCategory qc = questTemplate->getCategory();
		questEngine::model::QuestStatus s = qs->getStatus();

		if (qc == model::templates::quest::QuestCategory::QUEST && s != questEngine::model::QuestStatus::COMPLETE
			&& s != questEngine::model::QuestStatus::LOCKED) {
			questList.push_back(qs);
		}
	}
	return questList;
}

} // namespace aion::gameserver::model::gameobjects::player
