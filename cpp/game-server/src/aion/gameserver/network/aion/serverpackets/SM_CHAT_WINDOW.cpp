#include "aion/gameserver/network/aion/serverpackets/SM_CHAT_WINDOW.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CHAT_WINDOW::SM_CHAT_WINDOW(model::gameobjects::player::Player& targetValue, bool isGroupValue)
	: AionServerPacket(opcodeOf<SM_CHAT_WINDOW>), target(targetValue), isGroup(isGroupValue) {
}

SM_CHAT_WINDOW::~SM_CHAT_WINDOW() = default;

void SM_CHAT_WINDOW::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
