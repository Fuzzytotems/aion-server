#include "aion/gameserver/network/aion/serverpackets/SM_QUIT_RESPONSE.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_QUIT_RESPONSE::SM_QUIT_RESPONSE()
	: AionServerPacket(opcodeOf<SM_QUIT_RESPONSE>) {
}

SM_QUIT_RESPONSE::SM_QUIT_RESPONSE(bool edit_modeValue)
	: AionServerPacket(opcodeOf<SM_QUIT_RESPONSE>), edit_mode(edit_modeValue) {
}

void SM_QUIT_RESPONSE::writeImpl(AionConnection* con) {
	writeD(edit_mode ? 2 : 1); // 1 normal, 2 plastic surgery/gender switch
	writeC(0); // unk
	writeD(-1); // unk 3.0
}

} // namespace aion::gameserver::network::aion::serverpackets
