#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player's answer to a re-identification result (C_ANSWER_REIDENTIFY): accept applies the pending tune result, decline drops it; an
 * attribute-only re-identification cannot be declined (it is applied and audited).
 * <p>
 * C++ only: `CM_TUNE_RESULTTestAccess` (tests/cm_lz/TunePacketsTest.cpp) reads the fields readImpl decoded, which Java keeps private without
 * getters.
 *
 * @author Estrayl
 */
class CM_TUNE_RESULT : public AionClientPacket {
	friend struct CM_TUNE_RESULTTestAccess;

private:
	int32_t itemObjectId{};
	bool hasAccepted{};

public:
	CM_TUNE_RESULT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
