#include "aion/gameserver/network/aion/serverpackets/SM_PRIVATE_STORE_NAME.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PrivateStore.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PRIVATE_STORE_NAME::SM_PRIVATE_STORE_NAME(model::gameobjects::player::Player& player)
	: AionServerPacket(opcodeOf<SM_PRIVATE_STORE_NAME>) {
	playerObjId = player.getObjectId();
	name = player.getStore()->getStoreMessage();
}

void SM_PRIVATE_STORE_NAME::writeImpl(AionConnection* con) {
	writeD(playerObjId);
	writeS(name);
}

} // namespace aion::gameserver::network::aion::serverpackets
