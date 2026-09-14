#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_NOTIFY.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_FRIEND_NOTIFY::SM_FRIEND_NOTIFY(int8_t codeValue, std::string_view nameValue)
	: AionServerPacket(opcodeOf<SM_FRIEND_NOTIFY>), code(codeValue), name(nameValue) {
}

void SM_FRIEND_NOTIFY::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
