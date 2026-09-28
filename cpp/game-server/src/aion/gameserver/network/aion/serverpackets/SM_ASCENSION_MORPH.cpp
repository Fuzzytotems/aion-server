#include "aion/gameserver/network/aion/serverpackets/SM_ASCENSION_MORPH.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ASCENSION_MORPH::SM_ASCENSION_MORPH(int32_t inascensionValue) : AionServerPacket(opcodeOf<SM_ASCENSION_MORPH>), inascension(inascensionValue) {
}

void SM_ASCENSION_MORPH::writeImpl(AionConnection* con) {
	writeC(inascension); // if inascension =0x01 morph.
	writeC(0x00); // new 2.0 Packet --- probably pet info?
}

} // namespace aion::gameserver::network::aion::serverpackets
