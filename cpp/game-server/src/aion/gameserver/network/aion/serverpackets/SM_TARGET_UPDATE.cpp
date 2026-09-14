#include "aion/gameserver/network/aion/serverpackets/SM_TARGET_UPDATE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TARGET_UPDATE::SM_TARGET_UPDATE(model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_TARGET_UPDATE>), player(playerValue) {
}

SM_TARGET_UPDATE::~SM_TARGET_UPDATE() = default;

void SM_TARGET_UPDATE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
