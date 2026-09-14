#include "aion/gameserver/network/aion/serverpackets/SM_QUIT_RESPONSE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_QUIT_RESPONSE::SM_QUIT_RESPONSE()
	: AionServerPacket(opcodeOf<SM_QUIT_RESPONSE>) {
}

SM_QUIT_RESPONSE::SM_QUIT_RESPONSE(bool edit_modeValue)
	: AionServerPacket(opcodeOf<SM_QUIT_RESPONSE>), edit_mode(edit_modeValue) {
}

void SM_QUIT_RESPONSE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
