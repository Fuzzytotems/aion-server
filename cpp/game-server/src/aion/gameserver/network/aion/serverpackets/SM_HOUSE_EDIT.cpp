#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_EDIT.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/HouseDecoration.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/UseableItemObject.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/PlaceableHouseObject.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::serverpackets {

// Java: LoggerFactory.getLogger(getClass()) inside writeImpl
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.serverpackets.SM_HOUSE_EDIT");

SM_HOUSE_EDIT::SM_HOUSE_EDIT(int32_t actionValue) : AionServerPacket(opcodeOf<SM_HOUSE_EDIT>), action(actionValue) {
}

SM_HOUSE_EDIT::SM_HOUSE_EDIT(int32_t actionValue, int32_t storeIdValue, int32_t itemObjectIdValue) : SM_HOUSE_EDIT(actionValue) {
	this->itemObjectId = itemObjectIdValue;
	this->storeId = storeIdValue;
}

SM_HOUSE_EDIT::SM_HOUSE_EDIT(int32_t actionValue, int32_t itemObjectIdValue, float xValue, float yValue, float zValue, int32_t rotationValue)
	: AionServerPacket(opcodeOf<SM_HOUSE_EDIT>), action(actionValue), itemObjectId(itemObjectIdValue), x(xValue), y(yValue), z(zValue),
	  rotation(rotationValue) {
}

void SM_HOUSE_EDIT::writeImpl(AionConnection* con) {
	if (con == nullptr)
		throw runtime::NullPointerException("SM_HOUSE_EDIT::writeImpl without a connection");
	runtime::Ptr<model::gameobjects::player::Player> player = con->getActivePlayer();
	if (!player)
		return;
	runtime::Ptr<model::house::House> house = player->getActiveHouse();
	runtime::Ptr<model::gameobjects::HouseObject> obj = house->getRegistry()->getObjectByObjId(itemObjectId);
	if (action == 3) { // Add item
		int32_t templateId;
		int32_t typeId = 0;
		if (!obj) {
			runtime::Ptr<model::gameobjects::HouseDecoration> deco = house->getRegistry()->getDecorByObjId(itemObjectId);
			if (!deco) {
				log.warn("House item with object ID " + std::to_string(itemObjectId) + " wasn't found in registry of " + house->toString());
				return;
			}
			templateId = deco->getTemplateId();
		} else {
			templateId = obj->getObjectTemplate()->getTemplateId();
			typeId = obj->getObjectTemplate()->getTypeId();
		}
		writeC(action);
		writeC(storeId);
		writeD(itemObjectId);
		writeD(templateId);
		writeD(!obj ? 0 : obj->secondsUntilExpiration());
		writeDyeInfo(!obj ? std::nullopt : obj->getColor());
		writeD(0); // expiration as for armor ?
		writeC(typeId);
		// Additional info about the usage
		if (runtime::Ptr<model::gameobjects::UseableItemObject> useable = runtime::as<model::gameobjects::UseableItemObject>(obj)) {
			writeD(player->getObjectId());
			useable->writeUsageData(getBuf());
		}
	} else if (action == 4) { // Remove from inventory
		writeC(action);
		writeC(storeId);
		writeD(itemObjectId);
	} else if (action == 5) { // Spawn or move object
		writeC(action);
		writeD(house->getAddress()->getId()); // if painted 0 ?
		writeD(player->getCommonData()->getPlayerObjId());
		writeD(itemObjectId);
		writeD(obj->getObjectTemplate()->getTemplateId());
		writeF(x);
		writeF(y);
		writeF(z);
		writeH(rotation);
		writeD(player->getHouseObjectCooldowns()->remainingSeconds(itemObjectId));
		writeD(obj->secondsUntilExpiration());
		writeDyeInfo(obj->getColor());
		writeD(0); // expiration as for armor ?
		writeC(obj->getObjectTemplate()->getTypeId());
		if (runtime::Ptr<model::gameobjects::UseableItemObject> useable = runtime::as<model::gameobjects::UseableItemObject>(obj)) {
			useable->writeUsageData(getBuf());
		}
	} else if (action == 7) { // Despawn object
		writeC(action);
		writeD(itemObjectId);
	} else
		writeC(action);
}

} // namespace aion::gameserver::network::aion::serverpackets
