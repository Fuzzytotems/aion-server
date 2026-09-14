#include "aion/gameserver/network/aion/serverpackets/SM_ASCENSION_MORPH.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ASCENSION_MORPH::SM_ASCENSION_MORPH(int32_t inascensionValue) : AionServerPacket(opcodeOf<SM_ASCENSION_MORPH>), inascension(inascensionValue) {
}

void SM_ASCENSION_MORPH::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
