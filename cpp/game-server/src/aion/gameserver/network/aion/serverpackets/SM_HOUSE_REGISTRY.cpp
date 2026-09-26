#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_REGISTRY.h"

#include "aion/gameserver/model/gameobjects/HouseDecoration.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/UseableItemObject.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/Building.h"
#include "aion/gameserver/model/templates/housing/PlaceableHouseObject.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::serverpackets {

SM_HOUSE_REGISTRY::SM_HOUSE_REGISTRY(int32_t actionValue) : AionServerPacket(opcodeOf<SM_HOUSE_REGISTRY>), action(actionValue) {
}

void SM_HOUSE_REGISTRY::writeImpl(AionConnection* con) {
	if (con == nullptr)
		throw runtime::NullPointerException("SM_HOUSE_REGISTRY::writeImpl without a connection");
	runtime::Ptr<model::gameobjects::player::Player> player = con->getActivePlayer();
	if (!player)
		return;
	runtime::Ptr<model::house::HouseRegistry> houseRegistry = player->getActiveHouse()->getRegistry();
	writeC(action);
	if (action == 1) { // Display registered objects
		if (!houseRegistry) {
			writeH(0);
			return;
		}
		std::vector<runtime::Ptr<model::gameobjects::HouseObject>> notSpawnedObjects = houseRegistry->getNotSpawnedObjects();
		writeH(static_cast<int32_t>(notSpawnedObjects.size()));
		for (runtime::Ptr<model::gameobjects::HouseObject> obj : notSpawnedObjects) {
			if (!obj)
				continue;
			writeD(obj->getObjectId());
			int32_t templateId = obj->getObjectTemplate()->getTemplateId();
			writeD(templateId);
			writeD(player->getHouseObjectCooldowns()->remainingSeconds(obj->getObjectId()));
			writeD(obj->secondsUntilExpiration());
			writeDyeInfo(obj->getColor());
			writeD(0); // expiration as for armor ?
			writeC(obj->getObjectTemplate()->getTypeId());
			if (runtime::Ptr<model::gameobjects::UseableItemObject> useable = runtime::as<model::gameobjects::UseableItemObject>(obj)) {
				useable->writeUsageData(getBuf());
			}
		}
	} else if (action == 2) { // Display default and registered decoration items
		std::vector<int32_t> defaultDecorIds = houseRegistry->getOwner()->getBuilding()->getDefaultPartIds();
		std::vector<runtime::Ptr<model::gameobjects::HouseDecoration>> unusedDecors = houseRegistry->getUnusedDecors();
		writeH(static_cast<int32_t>(defaultDecorIds.size() + unusedDecors.size()));
		for (int32_t defaultPartId : defaultDecorIds) {
			writeD(0);
			writeD(defaultPartId);
		}
		for (runtime::Ptr<model::gameobjects::HouseDecoration> houseDecor : unusedDecors) {
			writeD(houseDecor->getObjectId());
			writeD(houseDecor->getTemplateId());
		}
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
