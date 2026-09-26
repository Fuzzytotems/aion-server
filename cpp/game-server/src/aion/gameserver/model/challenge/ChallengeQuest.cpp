#include "aion/gameserver/model/challenge/ChallengeQuest.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/templates/challenge/ChallengeQuestTemplate.h"

namespace aion::gameserver::model::challenge {

ChallengeQuest::ChallengeQuest(const templates::challenge::ChallengeQuestTemplate* value, int32_t completeCountValue)
	: template_(value), completeCount(completeCountValue) {
}

runtime::Ref<ChallengeQuest> ChallengeQuest::create(const templates::challenge::ChallengeQuestTemplate* value, int32_t completeCountValue) {
	return runtime::makeRef<ChallengeQuest>(value, completeCountValue);
}

int32_t ChallengeQuest::getQuestId() {
	AION_UNPORTED();
}

int32_t ChallengeQuest::getMaxRepeats() {
	AION_UNPORTED();
}

int32_t ChallengeQuest::getScorePerQuest() {
	AION_UNPORTED();
}

void ChallengeQuest::increaseCompleteCount() {
	AION_UNPORTED();
}

void ChallengeQuest::setPersistentState(gameobjects::Persistable::PersistentState value) {
	AION_UNPORTED();
}

ChallengeQuest::~ChallengeQuest() = default;

} // namespace aion::gameserver::model::challenge
