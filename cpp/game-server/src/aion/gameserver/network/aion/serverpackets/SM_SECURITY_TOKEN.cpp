#include "aion/gameserver/network/aion/serverpackets/SM_SECURITY_TOKEN.h"

#include <vector>

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SECURITY_TOKEN::SM_SECURITY_TOKEN(std::span<const uint8_t> tokenValue)
	: AionServerPacket(opcodeOf<SM_SECURITY_TOKEN>), token(tokenValue.begin(), tokenValue.end()) {
}

void SM_SECURITY_TOKEN::writeImpl(AionConnection* con) {
	writeC(0x0); // NA(0),EU(3)
	writeB(token);
	writeB(std::vector<uint8_t>(token.size()));
}

} // namespace aion::gameserver::network::aion::serverpackets
