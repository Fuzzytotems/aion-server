#include "aion/gameserver/network/aion/serverpackets/SM_STATUPDATE_EXP.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_STATUPDATE_EXP::SM_STATUPDATE_EXP(int64_t currentExpValue, int64_t recoverableExpValue, int64_t maxExpValue, int64_t rep1, int64_t rep2)
	: AionServerPacket(opcodeOf<SM_STATUPDATE_EXP>), currentExp(currentExpValue), recoverableExp(recoverableExpValue), maxExp(maxExpValue),
	  curBoostExp(rep1), maxBoostExp(rep2) {
}

void SM_STATUPDATE_EXP::writeImpl(AionConnection* con) {
	writeQ(currentExp);
	writeQ(recoverableExp);
	writeQ(maxExp);
	writeQ(curBoostExp);
	writeQ(maxBoostExp);
}

} // namespace aion::gameserver::network::aion::serverpackets
