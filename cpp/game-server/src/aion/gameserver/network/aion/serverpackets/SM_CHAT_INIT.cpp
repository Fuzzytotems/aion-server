#include "aion/gameserver/network/aion/serverpackets/SM_CHAT_INIT.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CHAT_INIT::SM_CHAT_INIT(std::span<const uint8_t> tokenValue)
	: AionServerPacket(opcodeOf<SM_CHAT_INIT>),
	  token(reinterpret_cast<const int8_t*>(tokenValue.data()), reinterpret_cast<const int8_t*>(tokenValue.data()) + tokenValue.size()) {
}

void SM_CHAT_INIT::writeImpl(AionConnection* con) {
	writeD(static_cast<int32_t>(token.size()));
	writeB(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(token.data()), token.size()));
}

} // namespace aion::gameserver::network::aion::serverpackets
