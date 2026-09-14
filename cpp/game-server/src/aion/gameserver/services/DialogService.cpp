#include "aion/gameserver/services/DialogService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.DialogService");

void DialogService::onCloseDialog(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::VisibleObject> target) {
	AION_UNPORTED();
}

// anonymous RequestResponseHandler at DialogService.java:135 (fieldmap key DialogService$1); local responseHandler; storage: stored in ResponseRequester
void DialogService::onDialogSelect(int32_t dialogActionId, model::gameobjects::player::Player& player, model::gameobjects::Npc& npc, int32_t questId, int32_t extendedRewardIndex) {
	AION_UNPORTED();
}

void DialogService::handleQuestDialogueOrSendNextPage(int32_t dialogActionId, model::gameobjects::player::Player& player, model::gameobjects::Npc& npc, int32_t questId, int32_t extendedRewardIndex) {
	AION_UNPORTED();
}

void DialogService::sendDialogWindow(int32_t dialogActionId, model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

bool DialogService::isInteractionAllowed(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

bool DialogService::isSummonOwner(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

bool DialogService::isSubDialogRestricted(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
