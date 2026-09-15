#include "aion/gameserver/network/aion/serverpackets/SM_EXCHANGE_REQUEST.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_EXCHANGE_REQUEST::SM_EXCHANGE_REQUEST(std::string_view receiverValue) : AionServerPacket(opcodeOf<SM_EXCHANGE_REQUEST>), receiver(receiverValue) {
}

void SM_EXCHANGE_REQUEST::writeImpl(AionConnection* con) {
	writeS(receiver);
}

} // namespace aion::gameserver::network::aion::serverpackets
