#include "aion/gameserver/network/aion/clientpackets/CM_CHAT_GROUP_INFO.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHAT_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_CHAT_GROUP_INFO::CM_CHAT_GROUP_INFO(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_CHAT_GROUP_INFO.java:26-30
void CM_CHAT_GROUP_INFO::readImpl() {
	playerName = readS();
	unk = readD();
}

// Java CM_CHAT_GROUP_INFO.java:32-40
void CM_CHAT_GROUP_INFO::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> target = world::World::getInstance().getPlayer(utils::ChatUtil::getRealCharName(playerName));
	if (target == nullptr) {
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER(playerName));
		return;
	}
	sendPacket(serverpackets::SM_CHAT_WINDOW(*target, true));
}

AION_CLIENT_PACKET(CM_CHAT_GROUP_INFO);

} // namespace aion::gameserver::network::aion::clientpackets
