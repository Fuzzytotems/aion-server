#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_OPEN_DOOR.h"

#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/controllers/HouseController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_HOUSE_OPEN_DOOR::CM_HOUSE_OPEN_DOOR(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_HOUSE_OPEN_DOOR.java:29-32
void CM_HOUSE_OPEN_DOOR::readImpl() {
	address = readD();
	leave = readC() != 0;
}

// Java CM_HOUSE_OPEN_DOOR.java:35-61
void CM_HOUSE_OPEN_DOOR::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (!player)
		return;

	const runtime::Ptr<model::house::House> house = services::HousingService::getInstance().getHouseByAddress(address);
	if (!house)
		return;

	if (leave) {
		const model::templates::housing::HouseAddress* houseAddress = house->getAddress();
		if (houseAddress->getExitMapId()) {
			// Java unboxes the exit coordinates (a NullPointerException for a missing one)
			if (!houseAddress->getExitX() || !houseAddress->getExitY() || !houseAddress->getExitZ())
				throw runtime::NullPointerException("null exit coordinate of house address " + std::to_string(houseAddress->getId()));
			services::teleport::TeleportService::teleportTo(*player, *houseAddress->getExitMapId(), *houseAddress->getExitX(), *houseAddress->getExitY(),
				*houseAddress->getExitZ(), int8_t{0}, model::animations::TeleportAnimation::FADE_OUT_BEAM);
		} else {
			house->getController().teleportNearHouseDoor(*player, true);
		}
	} else {
		if (player->hasAccess(configs::administration::AdminConfig::HOUSE_SHOW_ADDRESS.load()))
			utils::PacketSendUtility::sendMessage(*player, "House address: " + std::to_string(address));
		if (!house->canEnter(*player)) {
			utils::PacketSendUtility::sendPacket(*player, serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_ENTER_NO_RIGHT2());
			return;
		}
		house->getController().teleportNearHouseDoor(*player, false);
	}
}

AION_CLIENT_PACKET(CM_HOUSE_OPEN_DOOR);

} // namespace aion::gameserver::network::aion::clientpackets
