#include "aion/gameserver/network/aion/serverpackets/SM_CHARACTER_LIST.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CHARACTER_LIST::SM_CHARACTER_LIST(int32_t playOk2Value) : AbstractPlayerInfoPacket(opcodeOf<SM_CHARACTER_LIST>), playOk2(playOk2Value) {
}

void SM_CHARACTER_LIST::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
