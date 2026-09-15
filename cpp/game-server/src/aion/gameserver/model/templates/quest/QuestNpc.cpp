#include "aion/gameserver/model/templates/quest/QuestNpc.h"

#include <vector>

#include "aion/gameserver/questEngine/QuestEngine.h"

namespace aion::gameserver::model::templates::quest {

QuestNpc::QuestNpc(int32_t npcIdValue, int32_t questRangeValue) : npcId(npcIdValue), questRange(questRangeValue) {
	// Java: the six collections are created here (C++: default member initializers)
}

QuestNpc::QuestNpc(int32_t npcIdValue) : QuestNpc(npcIdValue, 20) {}

QuestNpc::~QuestNpc() = default;

runtime::Ref<QuestNpc> QuestNpc::create(int32_t npcIdValue, int32_t questRangeValue) {
	return runtime::makeRef<QuestNpc>(npcIdValue, questRangeValue);
}

runtime::Ref<QuestNpc> QuestNpc::create(int32_t npcIdValue) {
	return runtime::makeRef<QuestNpc>(npcIdValue);
}

void QuestNpc::addOnQuestStart(int32_t questId) {
	if (!onQuestStart.contains(questId)) {
		onQuestStart.add(questId);
	}
}

void QuestNpc::addOnAttackEvent(int32_t questId) {
	if (!onAttackEvent.contains(questId)) {
		onAttackEvent.add(questId);
	}
}

void QuestNpc::addOnKillEvent(int32_t questId) {
	if (!onKillEvent.contains(questId)) {
		onKillEvent.add(questId);
		questEngine::QuestEngine::getInstance().registerCanAct(questId, npcId);
	}
}

void QuestNpc::addOnTalkEvent(int32_t questId) {
	if (!onTalkEvent.contains(questId)) {
		onTalkEvent.add(questId);
		questEngine::QuestEngine::getInstance().registerCanAct(questId, npcId);
	}
}

void QuestNpc::addOnAddAggroListEvent(int32_t questId) {
	if (!onAddAggroListEvent.contains(questId)) {
		onAddAggroListEvent.add(questId);
		questEngine::QuestEngine::getInstance().registerCanAct(questId, npcId);
	}
}

void QuestNpc::addOnAtDistanceEvent(int32_t questId) {
	if (!onAtDistanceEvent.contains(questId)) {
		onAtDistanceEvent.add(questId);
		questEngine::QuestEngine::getInstance().registerCanAct(questId, npcId);
	}
}

std::unordered_set<int32_t> QuestNpc::findAllRegisteredQuestIds(const std::function<bool(int32_t)>& questIdFilter) {
	std::unordered_set<int32_t> result;
	auto collect = [&](const std::vector<int32_t>& questIds) {
		for (int32_t questId : questIds) {
			if (questIdFilter(questId))
				result.insert(questId);
		}
	};
	// Java: Stream.of(onQuestStart, onTalkEvent, onAtDistanceEvent, onAddAggroListEvent, onAttackEvent, onKillEvent)
	collect(onQuestStart.snapshot());
	collect(onTalkEvent.snapshot());
	collect(onAtDistanceEvent.snapshot());
	collect(onAddAggroListEvent.snapshot());
	collect(onAttackEvent.snapshot());
	collect(onKillEvent.snapshot());
	return result;
}

} // namespace aion::gameserver::model::templates::quest
