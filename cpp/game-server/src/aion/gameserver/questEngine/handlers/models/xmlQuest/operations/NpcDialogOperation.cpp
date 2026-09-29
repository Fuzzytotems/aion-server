#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/NpcDialogOperation.h"

#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations {

using network::aion::serverpackets::SM_DIALOG_WINDOW;

void NpcDialogOperation::doOperate(model::QuestEnv& env) const {
	runtime::Ptr<gameserver::model::gameobjects::player::Player> player = env.getPlayer();
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> obj = env.getVisibleObject();
	int32_t qId = env.getQuestId();
	if (questId)
		qId = *questId;
	if (!obj) // Java: NullPointerException on obj.getObjectId()
		throw runtime::NullPointerException("env.getVisibleObject()");
	if (qId == 0)
		utils::PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(obj->getObjectId(), id));
	else
		utils::PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(obj->getObjectId(), id, qId));
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations
