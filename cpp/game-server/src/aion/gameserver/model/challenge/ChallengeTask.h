#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/challenge/fwd.h"
#include "aion/gameserver/model/templates/challenge/fwd.h"

namespace aion::gameserver::model::challenge {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author ViAl
 */
class ChallengeTask : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t taskId;
	const int32_t ownerId;
	runtime::HashMap<int32_t, runtime::Ref<ChallengeQuest>> quests{AION_LOCK_CLASS(ChallengeTask::quests)};
	// fieldmap.toml: null until the task is completed (getCompleteTimeEpochSeconds tests it, ChallengeTask.java:82)
	runtime::Field<std::optional<commons::database::Timestamp>> completeTime{};
	const templates::challenge::ChallengeTaskTemplate* template_;

protected:
	/** Used for loading tasks from DAO. */
	/** @param quests stored by the task (Java keeps the map): moved in (hub-headers.md §7.1) */
	ChallengeTask(int32_t taskId, int32_t ownerId, std::unordered_map<int32_t, runtime::Ref<ChallengeQuest>> quests,
		std::optional<commons::database::Timestamp> completeTime);

public:
	static runtime::Ref<ChallengeTask> create(int32_t value, int32_t ownerIdValue, std::unordered_map<int32_t, runtime::Ref<ChallengeQuest>> questsValue,
		std::optional<commons::database::Timestamp> completeTimeValue);

protected:
	/** Used for creating new tasks in runtime. */
	ChallengeTask(int32_t ownerId, const templates::challenge::ChallengeTaskTemplate* template_);

public:
	static runtime::Ref<ChallengeTask> create(int32_t value, const templates::challenge::ChallengeTaskTemplate* template_Value);

	int32_t getTaskId() const { return this->taskId; }

	int32_t getOwnerId() const { return this->ownerId; }

	int32_t getQuestsCount();

	runtime::HashMap<int32_t, runtime::Ref<ChallengeQuest>>& getQuests() { return this->quests; }

	runtime::Ptr<ChallengeQuest> getQuest(int32_t questId);

	std::optional<commons::database::Timestamp> getCompleteTime() const { return this->completeTime.get(); }

	int32_t getCompleteTimeEpochSeconds();

	void updateCompleteTime(); // synchronized

	const templates::challenge::ChallengeTaskTemplate* getTemplate() const { return this->template_; }

	bool isCompleted();

protected:
	~ChallengeTask() override;
};

} // namespace aion::gameserver::model::challenge
