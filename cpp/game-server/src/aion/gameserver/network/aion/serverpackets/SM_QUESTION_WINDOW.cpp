#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

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
	writeD(code);
	for (size_t i = 0; i < MAX_PARAM_COUNT; i++) // client always wants content here, even if there is none
		writeS(i < params.size() ? params[i] : std::string()); // Java writeS(null) writes only the terminator
	writeD(0x00); // unk
	writeC(rangeOrCooldownSeconds > 0 ? 1 : 0); // 1 = check for range (client will auto decline) / display cooldown time
	writeD(senderId);
	writeD(rangeOrCooldownSeconds); // range within the question is valid or artifact/repair stone cooldown to display
}

} // namespace aion::gameserver::network::aion::serverpackets
