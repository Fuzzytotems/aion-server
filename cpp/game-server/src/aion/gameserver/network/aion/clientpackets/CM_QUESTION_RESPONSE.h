#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Response to SM_QUESTION_WINDOW (C_ANSWER): the answer is handed to the request the player's ResponseRequester holds under the question id;
 * a "yes" given while trading cancels the exchange first.
 * <p>
 * C++ only: `CM_QUESTION_RESPONSETestAccess` (tests/cm_lz/DialogPacketsTest.cpp) reads the fields readImpl decoded, which Java keeps private
 * without getters.
 *
 * @author Ben, Sarynth, Neon
 */
class CM_QUESTION_RESPONSE : public AionClientPacket {
	friend struct CM_QUESTION_RESPONSETestAccess;

private:
	int32_t questionid{};
	int32_t response{};
	int32_t senderid{}; // Java: @SuppressWarnings("unused")

public:
	CM_QUESTION_RESPONSE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
