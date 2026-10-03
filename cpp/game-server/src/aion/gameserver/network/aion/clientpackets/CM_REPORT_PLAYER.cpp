#include "aion/gameserver/network/aion/clientpackets/CM_REPORT_PLAYER.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_REPORT_PLAYER");

using model::gameobjects::player::Player;
using serverpackets::SM_SYSTEM_MESSAGE;

CM_REPORT_PLAYER::CM_REPORT_PLAYER(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_REPORT_PLAYER.java:30-33
void CM_REPORT_PLAYER::readImpl() {
	reportType = readUC();
	playerName = readS(); // the name of the reported person.
}

// Java CM_REPORT_PLAYER.java:36-56
void CM_REPORT_PLAYER::runImpl() {
	switch (reportType) {
		case 0: { // /accuse, /AutoReportHunting
			const runtime::Ptr<Player> activePlayer = getConnection()->getActivePlayer();
			const runtime::Ptr<Player> player = world::World::getInstance().getPlayer(utils::ChatUtil::getRealCharName(playerName));
			if (player != nullptr && player->getRace() != activePlayer->getRace()) {
				sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_DO_NOT_ACCUSE());
			} else if (player.rawPointer() == activePlayer.rawPointer()) {
				sendPacket(SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
			} else {
				utils::audit::AuditLogger::log(*activePlayer, "reported player " + playerName);
				sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_SUBMIT(playerName, "∞"));
			}
			break;
		}
		case 1: // /NumberofReports
			sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_COUNT_INFO("∞"));
			break;
		default:
			log.warn("Unhandled report type " + std::to_string(reportType) + " (reported player: " + playerName + ")");
	}
}

AION_CLIENT_PACKET(CM_REPORT_PLAYER);

} // namespace aion::gameserver::network::aion::clientpackets
