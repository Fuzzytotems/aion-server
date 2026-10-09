#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A used house object released (C_RELEASE_OBJECT): a running use is cancelled.
 * <p>
 * C++ only: `CM_RELEASE_OBJECTTestAccess` (tests/cm_lz) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Rolandas, Neon
 */
class CM_RELEASE_OBJECT : public AionClientPacket {
	friend struct CM_RELEASE_OBJECTTestAccess;

private:
	int32_t targetObjectId{};

public:
	CM_RELEASE_OBJECT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
