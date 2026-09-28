#include "aion/gameserver/network/aion/serverpackets/SM_CHARACTER_SELECT.h"

#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CHARACTER_SELECT::SM_CHARACTER_SELECT(int32_t typeValue) : AionServerPacket(opcodeOf<SM_CHARACTER_SELECT>), type(typeValue) {
}

SM_CHARACTER_SELECT::SM_CHARACTER_SELECT(int32_t typeValue, int16_t messageTypeValue, int32_t wrongCountValue)
	: AionServerPacket(opcodeOf<SM_CHARACTER_SELECT>), type(typeValue), messageType(messageTypeValue), wrongCount(wrongCountValue) {
}

void SM_CHARACTER_SELECT::writeImpl(AionConnection* con) {
	writeC(type);
	switch (type) {
		case 0:
			break;
		case 1:
			break;
		case 2:
			writeH(messageType); // 0: newpasskey complete, 2: passkey edit complete, 3: passkey input
			writeC(wrongCount > 0 ? 1 : 0); // 0: right passkey, 1: wrong passkey
			writeD(wrongCount); // wrong passkey input count
			// Enter the number of possible wrong numbers (retail server default value: 5)
			writeD(configs::main::SecurityConfig::PASSKEY_WRONG_MAXCOUNT.load());
			break;
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
