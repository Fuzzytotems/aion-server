#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player identifies an unidentified item, or re-tunes an identified one with a tuning scroll (C_IDENTIFY_ITEM).
 * <p>
 * C++ only: `CM_TUNETestAccess` (tests/cm_lz/TunePacketsTest.cpp) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author xTz
 */
class CM_TUNE : public AionClientPacket {
	friend struct CM_TUNETestAccess;

private:
	int32_t itemObjectId{}, tuningScrollObjectId{};

public:
	CM_TUNE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
