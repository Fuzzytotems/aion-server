#include "aion/gameserver/network/aion/serverpackets/SM_QUESTIONNAIRE.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_QUESTIONNAIRE::SM_QUESTIONNAIRE(int32_t messageIdValue, int8_t chunkValue, int8_t countValue, std::string_view htmlValue)
	: AionServerPacket(opcodeOf<SM_QUESTIONNAIRE>), messageId(messageIdValue), chunk(chunkValue), count(countValue), html(htmlValue) {
}

void SM_QUESTIONNAIRE::writeImpl(AionConnection* con) {
	writeD(messageId);
	writeC(chunk);
	writeC(count);
	writeH(commons::utils::StringUtils::utf16Length(html) * 2); // Java String.length(): UTF-16 code units
	writeS(html);
}

} // namespace aion::gameserver::network::aion::serverpackets
