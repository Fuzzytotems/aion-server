#include "aion/gameserver/network/aion/serverpackets/SM_SELL_ITEM.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SELL_ITEM::SM_SELL_ITEM(model::gameobjects::Npc& npc)
	: AionServerPacket(opcodeOf<SM_SELL_ITEM>) {
	AION_UNPORTED();
}

void SM_SELL_ITEM::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
