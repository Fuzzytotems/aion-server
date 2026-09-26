#include "aion/gameserver/questEngine/model/QuestState.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::questEngine::model {

QuestState::QuestState(int32_t questIdValue, QuestStatus statusValue, int32_t questVarsValue, int32_t flags, int32_t completeCountValue,
	std::optional<commons::database::Timestamp> nextRepeatTimeValue, std::optional<int32_t> rewardValue,
	std::optional<commons::database::Timestamp> completeTimeValue)
	: questId(questIdValue), questVars(QuestVars::create(questVarsValue)), questFlags(flags), status(statusValue), completeCount(completeCountValue),
	  completeTime(completeTimeValue), nextRepeatTime(nextRepeatTimeValue), reward(rewardValue), persistentState(PersistentState::NEW) {
}

QuestState::QuestState(int32_t questIdValue, QuestStatus statusValue)
	: QuestState(questIdValue, statusValue, 0, 0, statusValue == QuestStatus::COMPLETE ? 1 : 0, std::nullopt, std::nullopt,
		  statusValue == QuestStatus::COMPLETE ? std::optional<commons::database::Timestamp>(commons::database::Timestamp(
													 std::chrono::milliseconds(commons::utils::currentTimeMillis())))
											   : std::nullopt) {
}

QuestState::~QuestState() = default;

runtime::Ref<QuestState> QuestState::create(int32_t questIdValue, QuestStatus statusValue, int32_t questVarsValue, int32_t flags,
	int32_t completeCountValue, std::optional<commons::database::Timestamp> nextRepeatTimeValue, std::optional<int32_t> rewardValue,
	std::optional<commons::database::Timestamp> completeTimeValue) {
	return runtime::makeRef<QuestState>(questIdValue, statusValue, questVarsValue, flags, completeCountValue, nextRepeatTimeValue, rewardValue,
		completeTimeValue);
}

runtime::Ref<QuestState> QuestState::create(int32_t questIdValue, QuestStatus statusValue) {
	return runtime::makeRef<QuestState>(questIdValue, statusValue);
}

void QuestState::setQuestVarById(int32_t id, int32_t var) {
	AION_UNPORTED();
}

int32_t QuestState::getQuestVarById(int32_t id) {
	AION_UNPORTED();
}

void QuestState::setQuestVar(int32_t var) {
	AION_UNPORTED();
}

void QuestState::setStatus(QuestStatus value) {
	AION_UNPORTED();
}

void QuestState::setStatus(QuestStatus value, bool updateCompleteCountAndTime) {
	AION_UNPORTED();
}

void QuestState::setCompleteCount(int32_t value) {
	AION_UNPORTED();
}

void QuestState::setRewardGroup(std::optional<int32_t> value) {
	AION_UNPORTED();
}

bool QuestState::isStartable() {
	AION_UNPORTED();
}

bool QuestState::canRepeat() {
	AION_UNPORTED();
}

void QuestState::setPersistentState(PersistentState value) {
	AION_UNPORTED();
}

void QuestState::setFlags(int32_t value) {
	AION_UNPORTED();
}

int32_t QuestState::getStepGroup() {
	AION_UNPORTED();
}

void QuestState::setStepGroup(int32_t groupNumber) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::questEngine::model
