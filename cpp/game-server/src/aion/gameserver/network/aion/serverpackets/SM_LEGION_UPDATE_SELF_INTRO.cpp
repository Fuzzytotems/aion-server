#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_SELF_INTRO.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_UPDATE_SELF_INTRO::SM_LEGION_UPDATE_SELF_INTRO(int32_t playerObjIdValue, std::string_view selfintroValue)
	: AionServerPacket(opcodeOf<SM_LEGION_UPDATE_SELF_INTRO>), selfintro(selfintroValue), playerObjId(playerObjIdValue) {
}

void SM_LEGION_UPDATE_SELF_INTRO::writeImpl(AionConnection* con) {
	writeD(playerObjId);
	writeS(selfintro);
}

} // namespace aion::gameserver::network::aion::serverpackets
