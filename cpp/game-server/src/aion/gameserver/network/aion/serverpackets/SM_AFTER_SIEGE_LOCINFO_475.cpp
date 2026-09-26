#include "aion/gameserver/network/aion/serverpackets/SM_AFTER_SIEGE_LOCINFO_475.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_AFTER_SIEGE_LOCINFO_475::SM_AFTER_SIEGE_LOCINFO_475() : AionServerPacket(opcodeOf<SM_AFTER_SIEGE_LOCINFO_475>) {
}

void SM_AFTER_SIEGE_LOCINFO_475::writeImpl(AionConnection* con) {
	writeH(0);
	writeC(0);
}

} // namespace aion::gameserver::network::aion::serverpackets
