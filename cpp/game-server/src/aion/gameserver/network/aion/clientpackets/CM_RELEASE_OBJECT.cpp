#include "aion/gameserver/network/aion/clientpackets/CM_RELEASE_OBJECT.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/PostboxObject.h"
#include "aion/gameserver/model/gameobjects/UseableHouseObject.h"
#include "aion/gameserver/model/gameobjects/UseableItemObject.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_USE_OBJECT.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {


CM_RELEASE_OBJECT::CM_RELEASE_OBJECT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_RELEASE_OBJECT.java:29-31
void CM_RELEASE_OBJECT::readImpl() {
	targetObjectId = readD();
}

// Java CM_RELEASE_OBJECT.java:34-45
void CM_RELEASE_OBJECT::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	const runtime::Ptr<model::gameobjects::VisibleObject> object = player->getKnownList().getObject(targetObjectId);
	const runtime::Ptr<model::gameobjects::UseableHouseObject> useableHouseObject =
		object ? runtime::as<model::gameobjects::UseableHouseObject>(*object) : nullptr;
	if (useableHouseObject && useableHouseObject->releaseOccupant(*player)) { // release object
		if (player->getController().hasScheduledTask(model::TaskId::HOUSE_OBJECT_USE) ||
			runtime::as<model::gameobjects::PostboxObject>(*object)) { // post box always sends the message
			if (runtime::as<model::gameobjects::UseableItemObject>(*object)) // reset visual use progress bar
				utils::PacketSendUtility::sendPacket(*player, serverpackets::SM_USE_OBJECT(player->getObjectId(), object->getObjectId(), 0, 9));
			sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_CANCEL_USE());
		}
		player->getController().cancelTask(model::TaskId::HOUSE_OBJECT_USE);
	}
}

AION_CLIENT_PACKET(CM_RELEASE_OBJECT);

} // namespace aion::gameserver::network::aion::clientpackets
