#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/questEngine/model/QuestState.h"

namespace aion::gameserver::model::gameobjects::player {

[[maybe_unused]] static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.gameobjects.player.QuestStateList");

QuestStateList::QuestStateList() = default;

QuestStateList::~QuestStateList() = default;

runtime::Ref<QuestStateList> QuestStateList::create() {
	return runtime::makeRef<QuestStateList>();
}

bool QuestStateList::hasQuest(int32_t questId) {
	AION_UNPORTED();
}

bool QuestStateList::addQuest(int32_t questId, questEngine::model::QuestState& questState) {
	AION_UNPORTED();
}

runtime::Ptr<questEngine::model::QuestState> QuestStateList::deleteQuest(int32_t questId) {
	AION_UNPORTED();
}

runtime::Ptr<questEngine::model::QuestState> QuestStateList::getQuestState(int32_t questId) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<questEngine::model::QuestState>> QuestStateList::getAllQuestState() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<questEngine::model::QuestState>> QuestStateList::getCompletedQuests() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<questEngine::model::QuestState>> QuestStateList::getUncompletedQuests() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<questEngine::model::QuestState>> QuestStateList::getNormalQuests() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
