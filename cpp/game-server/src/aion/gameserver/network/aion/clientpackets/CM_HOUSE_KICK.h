#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The visitors of the own house sent out, friends included or not (C_HOUSING_KICK).
 * <p>
 * C++ only: `CM_HOUSE_KICKTestAccess` (tests/cm_lz) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Rolandas
 */
class CM_HOUSE_KICK : public AionClientPacket {
	friend struct CM_HOUSE_KICKTestAccess;

private:
	int8_t option{};

public:
	CM_HOUSE_KICK(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
