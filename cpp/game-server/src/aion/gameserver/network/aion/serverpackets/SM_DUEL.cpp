#include "aion/gameserver/network/aion/serverpackets/SM_DUEL.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_DUEL::SM_DUEL(int32_t typeValue) : AionServerPacket(opcodeOf<SM_DUEL>), type(typeValue) {
}

SM_DUEL SM_DUEL::SM_DUEL_STARTED(int32_t requesterObjId) {
	SM_DUEL packet(0x00);
	packet.setRequesterObjId(requesterObjId);
	return packet;
}

SM_DUEL SM_DUEL::SM_DUEL_RESULT(model::DuelResult result, std::string_view playerName) {
	SM_DUEL packet(0x01);
	packet.setPlayerName(playerName);
	packet.setResult(result);
	return packet;
}

void SM_DUEL::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
