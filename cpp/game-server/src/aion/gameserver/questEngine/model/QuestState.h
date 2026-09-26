#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::questEngine::model {

/**
 * The state of one quest in a player's quest list (status, quest vars, flags, completion count and times).
 * <p>
 * Hub header (docs/design/hub-headers.md). RefCounted and Persistable; Java `new QuestState(...)` is `QuestState::create(...)`. The nullable
 * Timestamps (Java null) are `std::optional<commons::database::Timestamp>` (§6), in the fields too.
 *
 * @author MrPoke, vlog, Rolandas
 */
class QuestState : public runtime::RefCounted, public gameserver::model::gameobjects::Persistable {
	AION_MAKE_REF_FRIEND
private:
	const int32_t questId;
	const runtime::Ref<QuestVars> questVars;
	runtime::Field<int32_t> questFlags{};
	runtime::Field<QuestStatus> status{};
	runtime::Field<int32_t> completeCount{};
	runtime::Field<std::optional<commons::database::Timestamp>> completeTime{};
	runtime::Field<std::optional<commons::database::Timestamp>> nextRepeatTime{};
	runtime::Field<std::optional<int32_t>> reward{};
	runtime::Field<gameserver::model::gameobjects::Persistable::PersistentState> persistentState{};

protected:
	QuestState(int32_t questId, QuestStatus status, int32_t questVars, int32_t flags, int32_t completeCount,
		std::optional<commons::database::Timestamp> nextRepeatTime, std::optional<int32_t> reward,
		std::optional<commons::database::Timestamp> completeTime);

	QuestState(int32_t questId, QuestStatus status);

	~QuestState() override;

public:
	/** Java: new QuestState(questId, status, questVars, flags, completeCount, nextRepeatTime, reward, completeTime) */
	static runtime::Ref<QuestState> create(int32_t questId, QuestStatus status, int32_t questVars, int32_t flags, int32_t completeCount,
		std::optional<commons::database::Timestamp> nextRepeatTime, std::optional<int32_t> reward,
		std::optional<commons::database::Timestamp> completeTime);

	/** Java: new QuestState(questId, status) */
	static runtime::Ref<QuestState> create(int32_t questId, QuestStatus status);

	runtime::Ptr<QuestVars> getQuestVars() const { return questVars; }

	void setQuestVarById(int32_t id, int32_t var);

	int32_t getQuestVarById(int32_t id);

	void setQuestVar(int32_t var);

	QuestStatus getStatus() const { return status.get(); }

	void setStatus(QuestStatus status);

	void setStatus(QuestStatus status, bool updateCompleteCountAndTime);

	std::optional<commons::database::Timestamp> getLastCompleteTime() const { return completeTime.get(); }

	int32_t getQuestId() const { return questId; }

	int32_t getCompleteCount() const { return completeCount.get(); }

	void setCompleteCount(int32_t completeCount);

	void setNextRepeatTime(std::optional<commons::database::Timestamp> value) { nextRepeatTime.set(value); }

	std::optional<commons::database::Timestamp> getNextRepeatTime() const { return nextRepeatTime.get(); }

	void setRewardGroup(std::optional<int32_t> reward);

	/**
	 * @return The reward group or null if not set
	 */
	std::optional<int32_t> getRewardGroup() const { return reward.get(); }

	/**
	 * @return True, if the quest is not active or is complete and can currently be repeated.
	 */
	bool isStartable();

	bool canRepeat();

	PersistentState getPersistentState() override { return persistentState.get(); }

	void setPersistentState(PersistentState persistentState) override;

	/**
	 * Possibly it is the second set of quest vars, now are named as flags
	 *
	 * @return the questFlags
	 */
	int32_t getFlags() const { return questFlags.get(); }

	/**
	 * Possibly it is the second set of quest vars, now are named as flags
	 *
	 * @param questFlags
	 *          the questFlags to set
	 */
	void setFlags(int32_t questFlags);

	int32_t getStepGroup();

	void setStepGroup(int32_t groupNumber);
};

} // namespace aion::gameserver::questEngine::model
