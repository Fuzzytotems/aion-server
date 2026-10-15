#include "aion/gameserver/questEngine/model/QuestState.h"

#include <chrono>
#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::questEngine::model {

namespace {

/**
 * Java `new Timestamp(System.currentTimeMillis())`. C++: the clock of the installed scheduler backend - the system clock in the server, the
 * ManualClock under a DeterministicExecutor - which is also the clock QuestService::calculateRepeatDate reads, so a completion time and the
 * next repeat time canRepeat compares with it are on one time line (docs/deviations/P5-06a.md; the pattern of Effect.cpp and P5-02b.md).
 */
commons::database::Timestamp now() {
	return commons::database::Timestamp(std::chrono::milliseconds(utils::ThreadPoolManager::clock().currentTimeMillis()));
}

/** Java int arithmetic (two's complement wrap-around) */
constexpr int32_t javaIncrement(int32_t value) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(value) + 1u);
}

/** Java `int << n`: the bits shifted out of the int are dropped */
constexpr int32_t javaShiftLeft(int32_t value, int32_t n) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(value) << (n & 31));
}

} // namespace

QuestState::QuestState(int32_t questIdValue, QuestStatus statusValue, int32_t questVarsValue, int32_t flags, int32_t completeCountValue,
	std::optional<commons::database::Timestamp> nextRepeatTimeValue, std::optional<int32_t> rewardValue,
	std::optional<commons::database::Timestamp> completeTimeValue)
	: questId(questIdValue), questVars(QuestVars::create(questVarsValue)), questFlags(flags), status(statusValue), completeCount(completeCountValue),
	  completeTime(completeTimeValue), nextRepeatTime(nextRepeatTimeValue), reward(rewardValue), persistentState(PersistentState::NEW) {
}

QuestState::QuestState(int32_t questIdValue, QuestStatus statusValue)
	: QuestState(questIdValue, statusValue, 0, 0, statusValue == QuestStatus::COMPLETE ? 1 : 0, std::nullopt, std::nullopt,
		  statusValue == QuestStatus::COMPLETE ? std::optional<commons::database::Timestamp>(now()) : std::nullopt) {
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

// java-race: QuestState.java has no synchronization. Its setters are read-modify-writes of several fields (the variables and the persistent
// state, the status with the complete count and time) that the player's packet threads, the quest timers and the periodic save
// (PlayerQuestListDAO.store) may run side by side, as in Java; each Field access is atomic on its own, the sequence is not.

void QuestState::setQuestVarById(int32_t id, int32_t var) {
	questVars->setVarById(id, var);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

int32_t QuestState::getQuestVarById(int32_t id) {
	return questVars->getVarById(id);
}

void QuestState::setQuestVar(int32_t var) {
	questVars->setVar(var);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void QuestState::setStatus(QuestStatus value) {
	setStatus(value, true);
}

void QuestState::setStatus(QuestStatus value, bool updateCompleteCountAndTime) {
	if (value == QuestStatus::COMPLETE && status.get() != QuestStatus::COMPLETE && updateCompleteCountAndTime) {
		completeTime.set(now());
		completeCount.set(javaIncrement(completeCount.get())); // java-race: completeCount++ is not atomic in Java either
	}
	status.set(value);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void QuestState::setCompleteCount(int32_t value) {
	completeCount.set(value);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void QuestState::setRewardGroup(std::optional<int32_t> value) {
	reward.set(value);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

bool QuestState::isStartable() {
	return status.get() == QuestStatus::COMPLETE && canRepeat();
}

bool QuestState::canRepeat() {
	const gameserver::model::templates::QuestTemplate* template_ = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
	if (template_ == nullptr) // Java: template.getMaxRepeatCount() on a null template
		throw runtime::NullPointerException("QUEST_DATA.getQuestById(" + std::to_string(questId) + ")");
	if (completeCount.get() >= template_->getMaxRepeatCount() && template_->getMaxRepeatCount() != 255)
		return false;
	if (template_->isTimeBased() && nextRepeatTime.get()) {
		commons::database::Timestamp currentTime = now();
		if (currentTime < *nextRepeatTime.get()) // Java Timestamp.before
			return false;
	}
	return true;
}

void QuestState::setPersistentState(PersistentState value) {
	// Java: a switch with a fallthrough from UPDATE_REQUIRED into default (QuestState.java:139-154)
	switch (value) {
		case PersistentState::DELETED:
			if (persistentState.get() == PersistentState::NEW)
				persistentState.set(PersistentState::NOACTION);
			else
				persistentState.set(PersistentState::DELETED);
			break;
		case PersistentState::UPDATE_REQUIRED:
			if (persistentState.get() == PersistentState::NEW)
				break;
			[[fallthrough]];
		default:
			persistentState.set(value);
	}
}

void QuestState::setFlags(int32_t value) {
	questFlags.set(value);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

int32_t QuestState::getStepGroup() {
	return questFlags.get() >> 6; // Java >> on int: an arithmetic shift, as in C++20
}

void QuestState::setStepGroup(int32_t groupNumber) {
	setFlags(javaShiftLeft(groupNumber, 6));
}

} // namespace aion::gameserver::questEngine::model
