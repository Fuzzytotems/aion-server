#include "aion/gameserver/model/templates/quest/QuestNpc.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::templates::quest {

QuestNpc::QuestNpc(int32_t npcIdValue, int32_t questRangeValue) : npcId(npcIdValue), questRange(questRangeValue) {
	// Java: the six collections are created here (C++: default member initializers)
}

QuestNpc::QuestNpc(int32_t npcIdValue) : QuestNpc(npcIdValue, 20) {
}

QuestNpc::~QuestNpc() = default;

runtime::Ref<QuestNpc> QuestNpc::create(int32_t npcIdValue, int32_t questRangeValue) {
	return runtime::makeRef<QuestNpc>(npcIdValue, questRangeValue);
}

runtime::Ref<QuestNpc> QuestNpc::create(int32_t npcIdValue) {
	return runtime::makeRef<QuestNpc>(npcIdValue);
}

void QuestNpc::addOnQuestStart(int32_t questId) {
	AION_UNPORTED();
}

void QuestNpc::addOnAttackEvent(int32_t questId) {
	AION_UNPORTED();
}

void QuestNpc::addOnKillEvent(int32_t questId) {
	AION_UNPORTED();
}

void QuestNpc::addOnTalkEvent(int32_t questId) {
	AION_UNPORTED();
}

void QuestNpc::addOnAddAggroListEvent(int32_t questId) {
	AION_UNPORTED();
}

void QuestNpc::addOnAtDistanceEvent(int32_t questId) {
	AION_UNPORTED();
}

std::unordered_set<int32_t> QuestNpc::findAllRegisteredQuestIds(const std::function<bool(int32_t)>& questIdFilter) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::templates::quest
