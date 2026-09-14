#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_RESPONSE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ATTACK_RESPONSE SM_ATTACK_RESPONSE::TARGET_IN_DIFFERENT_AREA(int32_t count) {
	return SM_ATTACK_RESPONSE(1, count);
}

SM_ATTACK_RESPONSE SM_ATTACK_RESPONSE::STOP_INVALID_TARGET(int32_t count) {
	return SM_ATTACK_RESPONSE(2, count);
}

SM_ATTACK_RESPONSE SM_ATTACK_RESPONSE::TARGET_TOO_FAR_AWAY(int32_t count) {
	return SM_ATTACK_RESPONSE(4, count);
}

SM_ATTACK_RESPONSE SM_ATTACK_RESPONSE::STOP_OBSTACLE_IN_THE_WAY(int32_t count) {
	return SM_ATTACK_RESPONSE(5, count);
}

SM_ATTACK_RESPONSE SM_ATTACK_RESPONSE::STOP_TOO_CLOSE_TO_ATTACK(int32_t count) {
	return SM_ATTACK_RESPONSE(6, count);
}

SM_ATTACK_RESPONSE SM_ATTACK_RESPONSE::STOP_WITHOUT_MESSAGE(int32_t count) {
	return SM_ATTACK_RESPONSE(7, count);
}

SM_ATTACK_RESPONSE::SM_ATTACK_RESPONSE(int32_t messageValue, int32_t attackCountValue)
	: AionServerPacket(opcodeOf<SM_ATTACK_RESPONSE>), message(messageValue), attackCount(attackCountValue) {
}

void SM_ATTACK_RESPONSE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
