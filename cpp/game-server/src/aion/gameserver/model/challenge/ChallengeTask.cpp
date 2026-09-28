#include "aion/gameserver/model/challenge/ChallengeTask.h"

#include <utility>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/challenge/ChallengeQuest.h"
#include "aion/gameserver/model/templates/challenge/ChallengeTaskTemplate.h"

namespace aion::gameserver::model::challenge {

ChallengeTask::ChallengeTask(int32_t value, int32_t ownerIdValue, std::unordered_map<int32_t, runtime::Ref<ChallengeQuest>> questsValue,
	std::optional<commons::database::Timestamp> completeTimeValue)
	: taskId(value), ownerId(ownerIdValue), template_() {
	// Java: this.quests = quests; this.completeTime = completeTime; this.template = DataManager.CHALLENGE_DATA.getTaskByTaskId(taskId)
	AION_UNPORTED();
}

runtime::Ref<ChallengeTask> ChallengeTask::create(int32_t value, int32_t ownerIdValue, std::unordered_map<int32_t, runtime::Ref<ChallengeQuest>> questsValue,
	std::optional<commons::database::Timestamp> completeTimeValue) {
	return runtime::makeRef<ChallengeTask>(value, ownerIdValue, std::move(questsValue), completeTimeValue);
}

ChallengeTask::ChallengeTask(int32_t value, const templates::challenge::ChallengeTaskTemplate* template_Value)
	: taskId(), ownerId(value), template_(template_Value) {
	// Java: this.taskId = template.getId(); Map<Integer, ChallengeQuest> quests = new HashMap<>(); for (ChallengeQuestTemplate qt :
	// template.getQuests()) { ChallengeQuest quest = new ChallengeQuest(qt, 0); quest.setPersistentState(PersistentState.NEW); quests.put(qt.getId(),
	// quest); } this.quests = quests
	AION_UNPORTED();
}

runtime::Ref<ChallengeTask> ChallengeTask::create(int32_t value, const templates::challenge::ChallengeTaskTemplate* template_Value) {
	return runtime::makeRef<ChallengeTask>(value, template_Value);
}

int32_t ChallengeTask::getQuestsCount() {
	AION_UNPORTED();
}

runtime::Ptr<ChallengeQuest> ChallengeTask::getQuest(int32_t questId) {
	AION_UNPORTED();
}

int32_t ChallengeTask::getCompleteTimeEpochSeconds() {
	AION_UNPORTED();
}

void ChallengeTask::updateCompleteTime() {
	AION_UNPORTED();
}

bool ChallengeTask::isCompleted() {
	AION_UNPORTED();
}

ChallengeTask::~ChallengeTask() = default;

} // namespace aion::gameserver::model::challenge
