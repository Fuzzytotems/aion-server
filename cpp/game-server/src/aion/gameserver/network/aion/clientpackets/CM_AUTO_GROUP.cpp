#include "aion/gameserver/network/aion/clientpackets/CM_AUTO_GROUP.h"

#include <optional>

#include "aion/gameserver/configs/main/AutoGroupConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/autogroup/EntryRequestTypeInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/AutoGroupService.h"
#include "aion/gameserver/services/instance/PeriodicInstanceManager.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;

CM_AUTO_GROUP::CM_AUTO_GROUP(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_AUTO_GROUP.java:27-32
void CM_AUTO_GROUP::readImpl() {
	instanceMaskId = readD();
	windowId = static_cast<int8_t>(readC());
	entryRequestId = static_cast<int8_t>(readC());
}

// Java CM_AUTO_GROUP.java:34-67
void CM_AUTO_GROUP::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (!configs::main::AutoGroupConfig::AUTO_GROUP_ENABLE.load()) {
		utils::PacketSendUtility::sendMessage(*player, "Auto Group is disabled");
		return;
	}
	switch (windowId) {
		case 100: {
			std::optional<model::autogroup::EntryRequestType> ert = model::autogroup::getTypeById(entryRequestId);
			if (!ert) {
				return;
			}
			services::AutoGroupService::getInstance().startLooking(*player, instanceMaskId, *ert);
			break;
		}
		case 101:
			services::AutoGroupService::getInstance().cancelRegistration(*player, instanceMaskId);
			break;
		case 102:
			services::AutoGroupService::getInstance().pressEnter(*player, instanceMaskId);
			break;
		case 103:
			services::AutoGroupService::getInstance().cancelEnter(*player, instanceMaskId);
			break;
		case 104:
			// is sent if a player clicks the icon
			services::instance::PeriodicInstanceManager::getInstance().handleRequest(*player, instanceMaskId);
			break;
		case 105:
			// DredgionRegService.getInstance().failedEnterDredgion(player);
			break;
		default:
			break;
	}
}

AION_CLIENT_PACKET(CM_AUTO_GROUP);

} // namespace aion::gameserver::network::aion::clientpackets
