#include "aion/gameserver/network/aion/serverpackets/SM_AFTER_SIEGE_LOCINFO_475.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_AFTER_SIEGE_LOCINFO_475::SM_AFTER_SIEGE_LOCINFO_475() : AionServerPacket(opcodeOf<SM_AFTER_SIEGE_LOCINFO_475>) {
}

void SM_AFTER_SIEGE_LOCINFO_475::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
