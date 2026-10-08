#include "aion/gameserver/model/challenge/ChallengeQuest.h"

#include "aion/gameserver/model/templates/challenge/ChallengeQuestTemplate.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::model::challenge {

ChallengeQuest::ChallengeQuest(const templates::challenge::ChallengeQuestTemplate* value, int32_t completeCountValue)
	: template_(value), completeCount(completeCountValue) {
}

runtime::Ref<ChallengeQuest> ChallengeQuest::create(const templates::challenge::ChallengeQuestTemplate* value, int32_t completeCountValue) {
	return runtime::makeRef<ChallengeQuest>(value, completeCountValue);
}

int32_t ChallengeQuest::getQuestId() {
	return template_->getId();
}

int32_t ChallengeQuest::getMaxRepeats() {
	return template_->getRepeatCount();
}

int32_t ChallengeQuest::getScorePerQuest() {
	return template_->getScore();
}

void ChallengeQuest::increaseCompleteCount() {
	SYNCHRONIZED(*this) { // Java: public synchronized void increaseCompleteCount()
		this->completeCount.set(this->completeCount.get() + 1);
		setPersistentState(gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
	}
}

void ChallengeQuest::setPersistentState(gameobjects::Persistable::PersistentState value) {
	if (this->persistentState.get() == gameobjects::Persistable::PersistentState::NEW && value == gameobjects::Persistable::PersistentState::UPDATE_REQUIRED)
		return;
	this->persistentState.set(value);
}

ChallengeQuest::~ChallengeQuest() = default;

} // namespace aion::gameserver::model::challenge
