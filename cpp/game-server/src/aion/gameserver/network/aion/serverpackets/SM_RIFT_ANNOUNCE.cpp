#include "aion/gameserver/network/aion/serverpackets/SM_RIFT_ANNOUNCE.h"

#include "aion/gameserver/controllers/RVController.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_RIFT_ANNOUNCE::SM_RIFT_ANNOUNCE(const std::map<int32_t, int32_t>& riftsValue)
	: AionServerPacket(opcodeOf<SM_RIFT_ANNOUNCE>), actionId(0), rifts(riftsValue) {
}

SM_RIFT_ANNOUNCE::SM_RIFT_ANNOUNCE(bool gelkmarosValue, bool inggisonValue)
	: AionServerPacket(opcodeOf<SM_RIFT_ANNOUNCE>), actionId(1), gelkmaros(gelkmarosValue ? 1 : 0), inggison(inggisonValue ? 1 : 0) {
}

SM_RIFT_ANNOUNCE::SM_RIFT_ANNOUNCE(controllers::RVController& riftValue, bool isMaster)
	: AionServerPacket(opcodeOf<SM_RIFT_ANNOUNCE>), actionId(isMaster ? 2 : 3), rift(riftValue) {
}

SM_RIFT_ANNOUNCE::SM_RIFT_ANNOUNCE(int32_t objectIdValue)
	: AionServerPacket(opcodeOf<SM_RIFT_ANNOUNCE>), actionId(4), objectId(objectIdValue) {
}

SM_RIFT_ANNOUNCE::~SM_RIFT_ANNOUNCE() = default;

void SM_RIFT_ANNOUNCE::writeImpl(AionConnection* con) {
	switch (actionId) {
		case 0: // announce
			writeH(1 + (static_cast<int32_t>(rifts.size()) * 4)); // following byte length
			writeC(actionId);
			for (const auto& [key, value] : rifts) // Java rifts.values() of the TreeMap
				writeD(value);
			break;
		case 1: // silentera
			writeH(9); // following byte length
			writeC(actionId);
			writeD(gelkmaros);
			writeD(inggison);
			break;
		case 2:
			writeH(35); // following byte length
			writeC(actionId);
			writeD(rift->getOwner().getObjectId());
			writeD(detail::unbox(rift->getMaxEntries(), "RVController.getMaxEntries()"));
			writeD(rift->getRemainTime());
			writeD(detail::unbox(rift->getMinLevel(), "RVController.getMinLevel()"));
			writeD(detail::unbox(rift->getMaxLevel(), "RVController.getMaxLevel()"));
			writeF(rift->getOwner().getX());
			writeF(rift->getOwner().getY());
			writeF(rift->getOwner().getZ());
			writeRiftType();
			writeC(rift->isMaster() ? 1 : 0); // display | hide
			break;
		case 3:
			writeH(15); // following byte length
			writeC(actionId);
			writeD(rift->getOwner().getObjectId());
			writeD(rift->getUsedEntries());
			writeD(rift->getRemainTime());
			writeRiftType();
			writeC(0); // unk
			break;
		case 4: // rift despawn
			writeH(5); // following byte length
			writeC(actionId);
			writeD(objectId);
			break;
	}
}

void SM_RIFT_ANNOUNCE::writeRiftType() {
	// 1 vortex, 2, concert hall, 3 pangaea, 4 chaos rift, 5 infiltration rift
	if (rift->isVortex()) {
		writeC(1);
	} else if (rift->isVolatile()) {
		writeC(4);
	} else if (rift->isInvasion()) {
		writeC(5);
	} else {
		writeC(0);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
