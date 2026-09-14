#include "aion/gameserver/network/aion/serverpackets/SM_RIFT_ANNOUNCE.h"

#include "aion/gameserver/controllers/RVController.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

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
	AION_UNPORTED();
}

void SM_RIFT_ANNOUNCE::writeRiftType() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
