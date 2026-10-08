#include "aion/gameserver/services/StaticDoorService.h"

#include <string>

#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/StaticDoor.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorState.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.StaticDoorService");

StaticDoorService& StaticDoorService::getInstance() {
	static StaticDoorService instance; // Java SingletonHolder
	return instance;
}

// Java StaticDoorService.java:30-48
void StaticDoorService::openStaticDoor(model::gameobjects::player::Player& player, int32_t doorId) {
	runtime::Ptr<model::gameobjects::StaticDoor> door = getDoor(player, doorId);
	if (door == nullptr)
		return;
	int32_t keyId = door->getObjectTemplate()->getKeyId();

	if (player.hasAccess(configs::administration::AdminConfig::INSTANCE_DOOR_INFO.load()))
		utils::PacketSendUtility::sendMessage(player, "Door ID: " + std::to_string(doorId) + ", key ID: " + std::to_string(keyId));

	bool opened = false;
	SYNCHRONIZED(*door) {
		if (!door->isOpen() && checkStaticDoorKey(player, *door, keyId)) {
			door->setOpen(true);
			opened = true;
		}
	}
	if (opened)
		player.getPosition()->getWorldMapInstance()->getInstanceHandler()->onOpenDoor(doorId);
}

// Java StaticDoorService.java:50-56. `"..." + door.getStates()` is EnumSet.toString: "[A, B]" in ordinal order
void StaticDoorService::changeStaticDoorState(model::gameobjects::player::Player& player, int32_t doorId, bool open, int32_t state) {
	runtime::Ptr<model::gameobjects::StaticDoor> door = getDoor(player, doorId);
	if (door == nullptr)
		return;
	door->changeState(open, state);
	std::string states = "[";
	for (model::templates::staticdoor::StaticDoorState s : door->getStates()) {
		if (states.size() > 1)
			states += ", ";
		states += xml::EnumTraits<model::templates::staticdoor::StaticDoorState>::names[static_cast<size_t>(s)];
	}
	states += "]";
	utils::PacketSendUtility::sendMessage(player, "Door states now are: " + states);
}

// Java StaticDoorService.java:58-68
runtime::Ptr<model::gameobjects::StaticDoor> StaticDoorService::getDoor(model::gameobjects::player::Player& player, int32_t doorId) {
	runtime::Ptr<model::gameobjects::VisibleObject> object = player.getPosition()->getWorldMapInstance()->getObjectByStaticId(doorId);
	runtime::Ptr<model::gameobjects::StaticDoor> door = runtime::as<model::gameobjects::StaticDoor>(object);
	if (door == nullptr) {
		if (object == nullptr)
			log.warn("Door (ID: " + std::to_string(doorId) + ") is missing near " + player.getPosition()->toString());
		else
			log.warn("Door (ID: " + std::to_string(doorId) + ") is not a static door but " + object->toString());
		return nullptr;
	}
	return door;
}

// Java StaticDoorService.java:70-91
bool StaticDoorService::checkStaticDoorKey(model::gameobjects::player::Player& player, model::gameobjects::StaticDoor& door, int32_t keyId) {
	if (player.hasAccess(configs::administration::AdminConfig::INSTANCE_OPEN_DOORS.load()))
		return true;

	if (keyId == 0)
		return true;

	if (keyId == 1)
		return false;

	if (!door.isLocked()) {
		return true;
	}

	if (!player.getInventory().decreaseByItemId(keyId, 1)) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_OPEN_DOOR_NEED_KEY_ITEM());
		return false;
	}

	door.setLocked(false);

	return true;
}

} // namespace aion::gameserver::services
