#include "aion/gameserver/network/aion/serverpackets/SM_CLOSE_QUESTION_WINDOW.h"

#include <utility>

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CLOSE_QUESTION_WINDOW SM_CLOSE_QUESTION_WINDOW::STR_DUEL_REQUESTER_WITHDRAW_REQUEST(std::string_view value0) {
	return SM_CLOSE_QUESTION_WINDOW(1300134, value0);
}

SM_CLOSE_QUESTION_WINDOW SM_CLOSE_QUESTION_WINDOW::STR_DUEL_HE_REJECT_DUEL(std::string_view value0) {
	return SM_CLOSE_QUESTION_WINDOW(1300097, value0);
}

SM_CLOSE_QUESTION_WINDOW SM_CLOSE_QUESTION_WINDOW::CLOSE_QUESTION_WINDOW() {
	return SM_CLOSE_QUESTION_WINDOW(0);
}

SM_CLOSE_QUESTION_WINDOW::SM_CLOSE_QUESTION_WINDOW(int32_t msgIdValue, std::vector<std::string> paramsValue)
	: AionServerPacket(opcodeOf<SM_CLOSE_QUESTION_WINDOW>), msgId(msgIdValue), params(std::move(paramsValue)) {
}

void SM_CLOSE_QUESTION_WINDOW::writeImpl(AionConnection* con) {
	writeD(0); // maybe a target object id?
	writeD(msgId); // reason
	for (int32_t i = 0; i < MAX_PARAM_COUNT; i++) // client only supports three parameters in this package (fourth will not be rendered)
		writeS(i < static_cast<int32_t>(params.size()) ? std::string_view(params[static_cast<size_t>(i)]) : std::string_view()); // Java null: ""
	// unknown what follows here
}

} // namespace aion::gameserver::network::aion::serverpackets
