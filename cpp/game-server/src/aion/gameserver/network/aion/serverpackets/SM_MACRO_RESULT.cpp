#include "aion/gameserver/network/aion/serverpackets/SM_MACRO_RESULT.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_MACRO_RESULT SM_MACRO_RESULT::SM_MACRO_CREATED(0x00);
SM_MACRO_RESULT SM_MACRO_RESULT::SM_MACRO_DELETED(0x01);

SM_MACRO_RESULT::SM_MACRO_RESULT(int32_t codeValue)
	: AionServerPacket(opcodeOf<SM_MACRO_RESULT>), code(codeValue) {
}

void SM_MACRO_RESULT::writeImpl(AionConnection* con) {
	writeC(code); // read-only: SM_MACRO_CREATED and SM_MACRO_DELETED are shared static packets (hub-headers.md §12)
}

} // namespace aion::gameserver::network::aion::serverpackets
