#include "aion/gameserver/network/aion/serverpackets/SM_DUEL.h"

#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/model/DuelResultInfo.h"
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
	writeC(type);
	switch (type) {
		case 0x00:
			writeD(requesterObjId);
			break;
		case 0x01:
			writeC(model::getResultId(result)); // unknown
			writeD(model::getMsgId(result));
			writeS(playerName);
			break;
		case 0xE0:
			break;
		default:
			throw commons::utils::IllegalArgumentException("invalid SM_DUEL packet type " + std::to_string(type));
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
