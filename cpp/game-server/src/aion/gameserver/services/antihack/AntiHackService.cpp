#include "aion/gameserver/services/antihack/AntiHackService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::antihack {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.antihack.AntiHackService");

bool AntiHackService::canMove(model::gameobjects::player::Player& player, float x, float y, float z, int8_t type) {
	AION_UNPORTED();
}

bool AntiHackService::punish(model::gameobjects::player::Player& player, bool normalMovePacket, std::string_view message) {
	AION_UNPORTED();
}

void AntiHackService::moveBack(model::gameobjects::player::Player& player, bool normalMovePacket) {
	AION_UNPORTED();
}

void AntiHackService::checkAionBin(int32_t size, network::aion::AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::antihack
