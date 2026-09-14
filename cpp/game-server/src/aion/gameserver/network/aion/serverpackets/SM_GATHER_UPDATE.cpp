#include "aion/gameserver/network/aion/serverpackets/SM_GATHER_UPDATE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GATHER_UPDATE::SM_GATHER_UPDATE(const model::templates::gather::GatherableTemplate* template_, const model::templates::gather::Material* material,
	int32_t successValue, int32_t failureValue, int32_t actionValue, int32_t executionSpeedValue, int32_t delayValue)
	: AionServerPacket(opcodeOf<SM_GATHER_UPDATE>) {
	AION_UNPORTED();
}

void SM_GATHER_UPDATE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

void SM_GATHER_UPDATE::writeSystemMsgInfo(int32_t msgId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
