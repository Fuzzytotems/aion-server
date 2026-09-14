#include "aion/gameserver/network/aion/serverpackets/SM_BLOCK_RESPONSE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_BLOCK_RESPONSE::SM_BLOCK_RESPONSE(int32_t codeValue, std::string_view playerNameValue)
	: AionServerPacket(opcodeOf<SM_BLOCK_RESPONSE>), code(codeValue), playerName(playerNameValue) {
}

void SM_BLOCK_RESPONSE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
