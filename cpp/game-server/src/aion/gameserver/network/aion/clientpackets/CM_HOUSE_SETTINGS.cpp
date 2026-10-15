#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_SETTINGS.h"

#include <optional>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/controllers/HouseController.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseDoorState.h"
#include "aion/gameserver/model/house/HouseDoorStateInfo.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/network/aion/serverpackets/AbstractHouseInfoPacket.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_ACQUIRE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;
using model::house::HouseDoorState;
using serverpackets::SM_SYSTEM_MESSAGE;


CM_HOUSE_SETTINGS::CM_HOUSE_SETTINGS(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_HOUSE_SETTINGS.java:31-35
void CM_HOUSE_SETTINGS::readImpl() {
	doorState = readC();
	showOwnerName = readC() == 1;
	signNotice = readS();
}

// Java CM_HOUSE_SETTINGS.java:38-68
void CM_HOUSE_SETTINGS::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (!player)
		return;
	constexpr int32_t SIGN_NOTICE_MAX_LENGTH = serverpackets::AbstractHouseInfoPacket::SIGN_NOTICE_MAX_LENGTH;
	if (commons::utils::StringUtils::utf16Length(signNotice) > SIGN_NOTICE_MAX_LENGTH) { // client limits sign notices to 64 chars but technically it supports more
		utils::audit::AuditLogger::log(*player, "sent string with more than 64 chars for house notice: " + signNotice);
		signNotice = commons::utils::StringUtils::substring(signNotice, 0, SIGN_NOTICE_MAX_LENGTH);
	}
	const std::optional<HouseDoorState> doorStateValue = model::house::houseDoorStateOf(this->doorState);
	const runtime::Ptr<model::house::House> house = player->getActiveHouse();
	if (!doorStateValue)
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_HOUSE_SETTINGS")
			.warn("{} sent unknown door state {} for {}", player->toString(), this->doorState, house ? house->toString() : std::string("null"));
	else if (!house) // Java: house.setDoorState(doorState) on a null house
		throw runtime::NullPointerException("the player has no active house");
	else
		house->setDoorState(*doorStateValue);
	if (!house)
		throw runtime::NullPointerException("the player has no active house");
	house->setShowOwnerName(showOwnerName);
	house->setSignNotice(signNotice);
	sendPacket(serverpackets::SM_HOUSE_ACQUIRE(player->getObjectId(), house->getAddress()->getId(), true));
	house->getController().updateAppearance();
	if (doorStateValue == HouseDoorState::OPEN)
		sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_ORDER_OPEN_DOOR());
	else if (doorStateValue == HouseDoorState::CLOSED_EXCEPT_FRIENDS) {
		house->getController().kickVisitors(player, false, false);
		sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_ORDER_CLOSE_DOOR_WITHOUT_FRIENDS());
	} else if (doorStateValue == HouseDoorState::CLOSED) {
		house->getController().kickVisitors(player, true, false);
		sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_ORDER_CLOSE_DOOR_ALL());
	}
}

AION_CLIENT_PACKET(CM_HOUSE_SETTINGS);

} // namespace aion::gameserver::network::aion::clientpackets
