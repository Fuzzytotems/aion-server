#include "aion/gameserver/network/aion/serverpackets/SM_GF_WEBSHOP_TOKEN_RESPONSE.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GF_WEBSHOP_TOKEN_RESPONSE::SM_GF_WEBSHOP_TOKEN_RESPONSE(std::string_view tokenValue)
	: AionServerPacket(opcodeOf<SM_GF_WEBSHOP_TOKEN_RESPONSE>), token(tokenValue) {
}

void SM_GF_WEBSHOP_TOKEN_RESPONSE::writeImpl(AionConnection* con) {
	writeS(token, 32);
}

} // namespace aion::gameserver::network::aion::serverpackets
