#include "aion/gameserver/network/aion/serverpackets/SM_CAPTCHA.h"

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
	writeC(type);
	switch (type) {
		case 0x01:
			writeC(count);
			writeD(size);
			writeB(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size()));
			break;
		case 0x03:
			writeH(isCorrect ? 1 : 0);
			// time setting can't be extracted (retail server default value:3000 sec)
			writeD(banTime);
			break;
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
