#include "aion/gameserver/network/aion/serverpackets/SM_CAPTCHA.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CAPTCHA::SM_CAPTCHA(int32_t countValue, std::span<const uint8_t> dataValue)
	: AionServerPacket(opcodeOf<SM_CAPTCHA>), type(1), count(countValue), size(static_cast<int32_t>(dataValue.size())),
	  data(reinterpret_cast<const int8_t*>(dataValue.data()), reinterpret_cast<const int8_t*>(dataValue.data()) + dataValue.size()) {
}

SM_CAPTCHA::SM_CAPTCHA(bool isCorrectValue, int32_t banTimeValue)
	: AionServerPacket(opcodeOf<SM_CAPTCHA>), type(3), isCorrect(isCorrectValue), banTime(banTimeValue) {
}

void SM_CAPTCHA::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
