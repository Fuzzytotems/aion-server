#include "aion/gameserver/network/aion/serverpackets/SM_SECURITY_TOKEN.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SECURITY_TOKEN::SM_SECURITY_TOKEN(std::span<const uint8_t> tokenValue)
	: AionServerPacket(opcodeOf<SM_SECURITY_TOKEN>), token(tokenValue.begin(), tokenValue.end()) {
}

void SM_SECURITY_TOKEN::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
