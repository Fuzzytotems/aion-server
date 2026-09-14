#include "aion/gameserver/network/aion/serverpackets/SM_QUESTIONNAIRE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_QUESTIONNAIRE::SM_QUESTIONNAIRE(int32_t messageIdValue, int8_t chunkValue, int8_t countValue, std::string_view htmlValue)
	: AionServerPacket(opcodeOf<SM_QUESTIONNAIRE>), messageId(messageIdValue), chunk(chunkValue), count(countValue), html(htmlValue) {
}

void SM_QUESTIONNAIRE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
