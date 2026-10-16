#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_OBJECT.h"

#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/NpcObject.h"
#include "aion/gameserver/model/gameobjects/UseableItemObject.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/PlaceableHouseObject.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::serverpackets {

SM_HOUSE_OBJECT::SM_HOUSE_OBJECT(model::gameobjects::HouseObject& owner) : AionServerPacket(opcodeOf<SM_HOUSE_OBJECT>), houseObject(owner) {
}

SM_HOUSE_OBJECT::~SM_HOUSE_OBJECT() = default;

void SM_HOUSE_OBJECT::writeImpl(AionConnection* con) {
	if (!houseObject)
		return;
	if (con == nullptr)
		throw runtime::NullPointerException("SM_HOUSE_OBJECT::writeImpl without a connection");
	runtime::Ptr<model::gameobjects::player::Player> player = con->getActivePlayer();
	if (!player)
		return;
	// may be null if it's a DummyHouseObject
	runtime::Ptr<model::house::House> house = !houseObject->getRegistry() ? nullptr : houseObject->getRegistry()->getOwner();
	int32_t templateId = houseObject->getObjectTemplate()->getTemplateId();
	writeD(!house ? 0 : house->getAddress()->getId()); // if painted 0 ?
	writeD(!house ? 0 : house->getOwnerId()); // player which owns house
	writeD(houseObject->getObjectId()); // <outlet[X]> data in house scripts
	writeD(houseObject->getObjectId()); // <outDB[X]> data in house scripts (probably DB id), where [X] is number
	writeD(templateId);
	writeF(houseObject->getX());
	writeF(houseObject->getY());
	writeF(houseObject->getZ());
	writeH(houseObject->getRotation());
	writeD(player->getHouseObjectCooldowns()->remainingSeconds(houseObject->getObjectId()));
	writeD(houseObject->secondsUntilExpiration());
	writeDyeInfo(houseObject->getColor());
	writeD(0); // expiration as for armor ?
	int8_t typeId = houseObject->getObjectTemplate()->getTypeId();
	writeC(typeId);
	switch (typeId) {
		case 1: // Use item
			runtime::cast<model::gameobjects::UseableItemObject>(houseObject)->writeUsageData(getBuf());
			break;
		case 7: { // Npc type
			runtime::Ptr<model::gameobjects::NpcObject> npcObj = runtime::cast<model::gameobjects::NpcObject>(houseObject);
			writeD(npcObj->getNpcObjectId());
			break;
		}
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
