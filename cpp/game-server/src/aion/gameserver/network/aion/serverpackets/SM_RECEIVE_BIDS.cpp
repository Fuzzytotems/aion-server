#include "aion/gameserver/network/aion/serverpackets/SM_RECEIVE_BIDS.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_RECEIVE_BIDS::SM_RECEIVE_BIDS(int32_t unkValue)
	: AionServerPacket(opcodeOf<SM_RECEIVE_BIDS>), unk(unkValue) {
}

void SM_RECEIVE_BIDS::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
