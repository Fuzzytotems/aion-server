#include "aion/gameserver/network/aion/serverpackets/SM_KISK_UPDATE.h"

#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_KISK_UPDATE::SM_KISK_UPDATE(model::gameobjects::Kisk& kiskValue) : AionServerPacket(opcodeOf<SM_KISK_UPDATE>), kisk(kiskValue) {
}

SM_KISK_UPDATE::~SM_KISK_UPDATE() = default;

void SM_KISK_UPDATE::writeImpl(AionConnection* con) {
	writeD(kisk->getObjectId());
	writeD(kisk->getCreatorId());
	writeD(kisk->getUseMask());
	writeD(kisk->getCurrentMemberCount());
	writeD(kisk->getMaxMembers());
	writeD(kisk->getRemainingResurrects());
	writeD(kisk->getMaxRessurects());
	writeD(kisk->getRemainingLifetime());
}

} // namespace aion::gameserver::network::aion::serverpackets
