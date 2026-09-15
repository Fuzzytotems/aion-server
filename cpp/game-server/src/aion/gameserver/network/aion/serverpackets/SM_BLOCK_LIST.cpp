#include "aion/gameserver/network/aion/serverpackets/SM_BLOCK_LIST.h"

#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/BlockedPlayer.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::serverpackets {

SM_BLOCK_LIST::SM_BLOCK_LIST() : AionServerPacket(opcodeOf<SM_BLOCK_LIST>) {
}

void SM_BLOCK_LIST::writeImpl(AionConnection* con) {
	if (con == nullptr)
		throw runtime::NullPointerException("SM_BLOCK_LIST::writeImpl without a connection");
	runtime::Ptr<model::gameobjects::player::BlockList> list = con->getActivePlayer()->getBlockList();
	writeH(-list->getSize());
	writeC(0); // Unk
	for (runtime::Ptr<model::gameobjects::player::BlockedPlayer> player : *list) {
		writeS(player->getName());
		writeS(player->getReason());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
