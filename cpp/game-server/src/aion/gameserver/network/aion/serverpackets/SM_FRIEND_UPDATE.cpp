#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_UPDATE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::network::aion::serverpackets {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.serverpackets.SM_FRIEND_UPDATE");

SM_FRIEND_UPDATE::SM_FRIEND_UPDATE(int32_t friendObjIdValue) : AionServerPacket(opcodeOf<SM_FRIEND_UPDATE>), friendObjId(friendObjIdValue) {
}

void SM_FRIEND_UPDATE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
