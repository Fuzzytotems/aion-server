#include "aion/gameserver/network/aion/clientpackets/CM_UPGRADE_ARCADE.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/UpgradeArcadeService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_UPGRADE_ARCADE::CM_UPGRADE_ARCADE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_UPGRADE_ARCADE.java:23-27
void CM_UPGRADE_ARCADE::readImpl() {
	action = readC();
	sessionId = readD();
}

// Java CM_UPGRADE_ARCADE.java:29-57
void CM_UPGRADE_ARCADE::runImpl() {
	if (!configs::main::EventsConfig::ENABLE_EVENT_ARCADE.load())
		return;
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	switch (action) {
		case 0: // get start upgrade arcade info
			services::UpgradeArcadeService::getInstance().start(*player, sessionId);
			break;
		case 1: // open upgrade arcade
			services::UpgradeArcadeService::getInstance().open(*player);
			break;
		case 2: // try upgrade arcade
			services::UpgradeArcadeService::getInstance().startTry(*player);
			break;
		case 3: // get reward
			services::UpgradeArcadeService::getInstance().getReward(*player);
			break;
		case 4: // resume upgrade arcade
			services::UpgradeArcadeService::getInstance().resume(*player);
			break;
		case 5: // get reward list
			services::UpgradeArcadeService::getInstance().showRewardList(*player);
			break;
		default:
			commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_UPGRADE_ARCADE")
				.warn("Unhandled arcade action " + std::to_string(action));
	}
}

AION_CLIENT_PACKET(CM_UPGRADE_ARCADE);

} // namespace aion::gameserver::network::aion::clientpackets
