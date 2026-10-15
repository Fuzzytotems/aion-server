#include "aion/gameserver/network/aion/clientpackets/CM_CHANGE_CHANNEL.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_CHANGE_CHANNEL::CM_CHANGE_CHANNEL(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_CHANGE_CHANNEL.java:27-30
void CM_CHANGE_CHANNEL::readImpl() {
	channel = readD();
}

// Java CM_CHANGE_CHANNEL.java:32-42
void CM_CHANGE_CHANNEL::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> activePlayer = getConnection()->getActivePlayer();
	runtime::Ptr<world::WorldMapInstance> instance = activePlayer->getPosition()->getWorldMapInstance();
	if (configs::main::WorldConfig::WORLD_EMULATE_FASTTRACK.load() && !instance->isBeginnerInstance()) {
		const model::templates::world::WorldMapTemplate* template_ = instance->getTemplate();
		// channel index starts from there
		channel += template_->getTwinCount() - 1;
	}
	services::teleport::TeleportService::changeChannel(*activePlayer, channel);
}

AION_CLIENT_PACKET(CM_CHANGE_CHANNEL);

} // namespace aion::gameserver::network::aion::clientpackets
