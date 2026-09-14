#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/challenge/fwd.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/templates/challenge/fwd.h"

namespace aion::gameserver::model::challenge {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author ViAl
 */
class ChallengeQuest : public runtime::RefCounted, public gameobjects::Persistable {
	AION_MAKE_REF_FRIEND
private:
	const templates::challenge::ChallengeQuestTemplate* template_;
	runtime::Field<int32_t> completeCount{};
	runtime::Field<gameobjects::Persistable::PersistentState> persistentState{};

protected:
	ChallengeQuest(const templates::challenge::ChallengeQuestTemplate* template_, int32_t completeCount);

public:
	static runtime::Ref<ChallengeQuest> create(const templates::challenge::ChallengeQuestTemplate* value, int32_t completeCountValue);

	int32_t getQuestId();

	const templates::challenge::ChallengeQuestTemplate* getQuestTemplate() const { return this->template_; }

	int32_t getMaxRepeats();

	int32_t getScorePerQuest();

	int32_t getCompleteCount() const { return this->completeCount.get(); }

	void increaseCompleteCount(); // synchronized

	gameobjects::Persistable::PersistentState getPersistentState() override { return this->persistentState.get(); }

	void setPersistentState(gameobjects::Persistable::PersistentState persistentState) override;

protected:
	~ChallengeQuest() override;
};

} // namespace aion::gameserver::model::challenge
