#include "aion/gameserver/network/aion/serverpackets/SM_KISK_UPDATE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_KISK_UPDATE::SM_KISK_UPDATE(model::gameobjects::Kisk& kiskValue) : AionServerPacket(opcodeOf<SM_KISK_UPDATE>), kisk(kiskValue) {
}

SM_KISK_UPDATE::~SM_KISK_UPDATE() = default;

void SM_KISK_UPDATE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
