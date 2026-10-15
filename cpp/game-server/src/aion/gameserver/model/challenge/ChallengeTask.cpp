#include "aion/gameserver/model/challenge/ChallengeTask.h"

#include <utility>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/ChallengeData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/challenge/ChallengeQuest.h"
#include "aion/gameserver/model/templates/challenge/ChallengeQuestTemplate.h"
#include "aion/gameserver/model/templates/challenge/ChallengeTaskTemplate.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::model::challenge {

ChallengeTask::ChallengeTask(int32_t value, int32_t ownerIdValue, std::unordered_map<int32_t, runtime::Ref<ChallengeQuest>> questsValue,
	std::optional<commons::database::Timestamp> completeTimeValue)
	: taskId(value), ownerId(ownerIdValue), template_(dataholders::DataManager::CHALLENGE_DATA->getTaskByTaskId(value)) {
	// Java: this.quests = quests (the map is moved in, ChallengeTask.h); this.completeTime = completeTime; this.template = the task's template
	// (C++: initialized first, the member is const)
	for (auto& [questId, quest] : questsValue)
		this->quests.put(questId, std::move(quest));
	this->completeTime.set(completeTimeValue);
}

runtime::Ref<ChallengeTask> ChallengeTask::create(int32_t value, int32_t ownerIdValue, std::unordered_map<int32_t, runtime::Ref<ChallengeQuest>> questsValue,
	std::optional<commons::database::Timestamp> completeTimeValue) {
	return runtime::makeRef<ChallengeTask>(value, ownerIdValue, std::move(questsValue), completeTimeValue);
}

ChallengeTask::ChallengeTask(int32_t value, const templates::challenge::ChallengeTaskTemplate* template_Value)
	: taskId(template_Value->getId()), ownerId(value), template_(template_Value) {
	// Java: this.taskId = template.getId() (C++: initialized first, the member is const)
	for (const templates::challenge::ChallengeQuestTemplate& qt : template_Value->getQuests()) {
		runtime::Ref<ChallengeQuest> quest = ChallengeQuest::create(&qt, 0);
		quest->setPersistentState(gameobjects::Persistable::PersistentState::NEW);
		this->quests.put(qt.getId(), std::move(quest));
	}
	// Java: this.quests = quests (the new HashMap filled above); this.template = template (initialized first)
}

runtime::Ref<ChallengeTask> ChallengeTask::create(int32_t value, const templates::challenge::ChallengeTaskTemplate* template_Value) {
	return runtime::makeRef<ChallengeTask>(value, template_Value);
}

int32_t ChallengeTask::getQuestsCount() {
	return quests.size();
}

runtime::Ptr<ChallengeQuest> ChallengeTask::getQuest(int32_t questId) {
	return quests.get(questId);
}

int32_t ChallengeTask::getCompleteTimeEpochSeconds() {
	const std::optional<commons::database::Timestamp> time = completeTime.get();
	return !time ? 0 : static_cast<int32_t>(time->time_since_epoch().count() / 1000);
}

void ChallengeTask::updateCompleteTime() {
	SYNCHRONIZED(*this) { // Java: public synchronized void updateCompleteTime()
		completeTime.set(commons::database::Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis())));
	}
}

bool ChallengeTask::isCompleted() {
	bool isCompleted = true;
	for (const runtime::Ref<ChallengeQuest>& quest : quests.values()) {
		if (quest->getCompleteCount() < quest->getMaxRepeats()) {
			isCompleted = false;
			break;
		}
	}
	return isCompleted;
}

ChallengeTask::~ChallengeTask() = default;

} // namespace aion::gameserver::model::challenge
