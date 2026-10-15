#include "aion/gameserver/network/aion/clientpackets/CM_USE_HOUSE_OBJECT.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/controllers/PlaceableObjectController.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {


CM_USE_HOUSE_OBJECT::CM_USE_HOUSE_OBJECT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_USE_HOUSE_OBJECT.java:24-26
void CM_USE_HOUSE_OBJECT::readImpl() {
	itemObjectId = readD();
}

// Java CM_USE_HOUSE_OBJECT.java:29-39
void CM_USE_HOUSE_OBJECT::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (!player)
		return;
	const runtime::Ptr<model::gameobjects::VisibleObject> visObject = world::World::getInstance().findVisibleObject(itemObjectId);
	if (!visObject)
		return;
	if (const runtime::Ptr<model::gameobjects::HouseObject> houseObject = runtime::as<model::gameobjects::HouseObject>(*visObject)) {
		houseObject->getController().onDialogRequest(*player);
	}
}

AION_CLIENT_PACKET(CM_USE_HOUSE_OBJECT);

} // namespace aion::gameserver::network::aion::clientpackets
