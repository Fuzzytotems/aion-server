#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

std::string SM_QUESTION_WINDOW::toJavaString(int32_t value) {
	return std::to_string(value);
}

std::string SM_QUESTION_WINDOW::toJavaString(int64_t value) {
	return std::to_string(value);
}

SM_QUESTION_WINDOW::SM_QUESTION_WINDOW(int32_t codeValue, int32_t senderIdValue, int32_t rangeOrCooldownSecondsValue,
	std::vector<std::string> paramsValue)
	: AionServerPacket(opcodeOf<SM_QUESTION_WINDOW>), code(codeValue), senderId(senderIdValue), rangeOrCooldownSeconds(rangeOrCooldownSecondsValue) {
	if (paramsValue.size() > MAX_PARAM_COUNT)
		throw runtime::IllegalArgumentException("More than " + std::to_string(MAX_PARAM_COUNT) + " message parameters are not supported");
	params = std::move(paramsValue);
}

void SM_QUESTION_WINDOW::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
