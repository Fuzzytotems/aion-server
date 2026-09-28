#include "aion/gameserver/questEngine/model/QuestEnv.h"

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::questEngine::model {

QuestEnv::QuestEnv(runtime::Ptr<gameserver::model::gameobjects::VisibleObject> visibleObjectValue,
	gameserver::model::gameobjects::player::Player& playerValue, int32_t questIdValue)
	: QuestEnv(visibleObjectValue, playerValue, questIdValue, gameserver::model::DialogAction::NULL_) {
}

QuestEnv::QuestEnv(runtime::Ptr<gameserver::model::gameobjects::VisibleObject> visibleObjectValue,
	gameserver::model::gameobjects::player::Player& playerValue, int32_t questIdValue, int32_t dialogActionIdValue)
	: visibleObject(runtime::Ref<gameserver::model::gameobjects::VisibleObject>(visibleObjectValue)),
	  player(runtime::Ref<gameserver::model::gameobjects::player::Player>(playerValue)), questId(questIdValue), dialogActionId(dialogActionIdValue) {
}

QuestEnv::~QuestEnv() = default;

runtime::Ref<QuestEnv> QuestEnv::create(runtime::Ptr<gameserver::model::gameobjects::VisibleObject> visibleObjectValue,
	gameserver::model::gameobjects::player::Player& playerValue, int32_t questIdValue) {
	return runtime::makeRef<QuestEnv>(visibleObjectValue, playerValue, questIdValue);
}

runtime::Ref<QuestEnv> QuestEnv::create(runtime::Ptr<gameserver::model::gameobjects::VisibleObject> visibleObjectValue,
	gameserver::model::gameobjects::player::Player& playerValue, int32_t questIdValue, int32_t dialogActionIdValue) {
	return runtime::makeRef<QuestEnv>(visibleObjectValue, playerValue, questIdValue, dialogActionIdValue);
}

void QuestEnv::setPlayer(runtime::Ptr<gameserver::model::gameobjects::player::Player> value) {
	player.set(value);
}

void QuestEnv::setVisibleObject(runtime::Ptr<gameserver::model::gameobjects::VisibleObject> value) {
	visibleObject.set(value);
}

void QuestEnv::setQuestId(std::optional<int32_t> value) {
	if (!value)
		throw runtime::NullPointerException("QuestEnv.setQuestId(null)"); // Java: unboxing a null Integer
	questId.set(*value);
}

int32_t QuestEnv::getTargetId() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::questEngine::model
