#include "aion/gameserver/network/aion/clientpackets/CM_PLAYER_LISTENER.h"

#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/services/reward/WebRewardService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_PLAYER_LISTENER::CM_PLAYER_LISTENER(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_PLAYER_LISTENER::readImpl() {
}

void CM_PLAYER_LISTENER::runImpl() {
	// The whole body is behind `gameserver.web_rewards.enable`, which is false by default (gameserver.properties:94) and in the M5a profile.
	// With the key ON this reaches WebRewardService::sendAvailableRewards, which is still AION_UNPORTED (WebRewardService.cpp:29-31, P5-08):
	// the packet is answered by throwing, which AionClientPacket::run logs. That is the only unported body this handler can reach, and it is
	// reachable only through that configuration key - see the wave B report.
	if (configs::main::GSConfig::ENABLE_WEB_REWARDS.load())
		services::reward::WebRewardService::getInstance().sendAvailableRewards(getConnection()->getActivePlayer());
}

AION_CLIENT_PACKET(CM_PLAYER_LISTENER);

} // namespace aion::gameserver::network::aion::clientpackets
