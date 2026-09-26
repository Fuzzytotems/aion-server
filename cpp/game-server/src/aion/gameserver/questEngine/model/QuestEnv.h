#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::questEngine::model {

/**
 * The environment of a quest event: the player, the object he interacts with, the quest and the dialog action.
 * <p>
 * Hub header (docs/design/hub-headers.md). RefCounted (captured by tasks and stored by FollowingNpcCheckTask, runtime-architecture.md §3.1):
 * Java `new QuestEnv(...)` is `QuestEnv::create(...)`; `QuestEnv env(...)` on the stack does not compile.
 *
 * @author MrPoke
 */
class QuestEnv : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<gameserver::model::gameobjects::VisibleObject>> visibleObject{};
	runtime::Field<runtime::Ref<gameserver::model::gameobjects::player::Player>> player{};
	runtime::Field<int32_t> questId{};
	runtime::Field<int32_t> dialogActionId{};
	runtime::Field<bool> isDialogContinuationFromPreQuest_{};
	runtime::Field<int32_t> extendedRewardIndex{};

protected:
	QuestEnv(runtime::Ptr<gameserver::model::gameobjects::VisibleObject> visibleObject, gameserver::model::gameobjects::player::Player& player,
		int32_t questId);

	QuestEnv(runtime::Ptr<gameserver::model::gameobjects::VisibleObject> visibleObject, gameserver::model::gameobjects::player::Player& player,
		int32_t questId, int32_t dialogActionId);

	~QuestEnv() override;

public:
	/** Java: new QuestEnv(visibleObject, player, questId) */
	static runtime::Ref<QuestEnv> create(runtime::Ptr<gameserver::model::gameobjects::VisibleObject> visibleObject,
		gameserver::model::gameobjects::player::Player& player, int32_t questId);

	/** Java: new QuestEnv(visibleObject, player, questId, dialogActionId) */
	static runtime::Ref<QuestEnv> create(runtime::Ptr<gameserver::model::gameobjects::VisibleObject> visibleObject,
		gameserver::model::gameobjects::player::Player& player, int32_t questId, int32_t dialogActionId);

	/**
	 * @return the visibleObject
	 */
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> getVisibleObject() const { return visibleObject.get(); }

	/**
	 * @param visibleObject
	 *          the visibleObject to set
	 */
	void setVisibleObject(runtime::Ptr<gameserver::model::gameobjects::VisibleObject> visibleObject);

	/**
	 * @return the player
	 */
	runtime::Ptr<gameserver::model::gameobjects::player::Player> getPlayer() const { return player.get(); }

	/**
	 * @param player
	 *          the player to set
	 */
	void setPlayer(runtime::Ptr<gameserver::model::gameobjects::player::Player> player);

	/**
	 * @return the questId
	 */
	int32_t getQuestId() const { return questId.get(); }

	/**
	 * @param questId
	 *          the questId to set (Java Integer: unboxing null throws NullPointerException)
	 */
	void setQuestId(std::optional<int32_t> questId);

	int32_t getDialogActionId() const { return dialogActionId.get(); }

	void setDialogActionId(int32_t value) { dialogActionId.set(value); }

	bool isDialogContinuationFromPreQuest() const { return isDialogContinuationFromPreQuest_.get(); }

	void setDialogContinuationFromPreQuest(bool value) { isDialogContinuationFromPreQuest_.set(value); }

	/**
	 * @return the target template id, 0 if no target ({@link #getVisibleObject()}) is set
	 */
	int32_t getTargetId();

	void setExtendedRewardIndex(int32_t index) { extendedRewardIndex.set(index); }

	int32_t getExtendedRewardIndex() const { return extendedRewardIndex.get(); }
};

} // namespace aion::gameserver::questEngine::model
