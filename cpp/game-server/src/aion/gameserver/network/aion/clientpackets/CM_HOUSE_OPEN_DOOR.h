#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A house door used: entering (if the house lets the player in) or leaving (C_USE_HOUSING_DOOR).
 * <p>
 * C++ only: `CM_HOUSE_OPEN_DOORTestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Rolandas
 */
class CM_HOUSE_OPEN_DOOR : public AionClientPacket {
	friend struct CM_HOUSE_OPEN_DOORTestAccess;

private:
	int32_t address{};
	bool leave{};

public:
	CM_HOUSE_OPEN_DOOR(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
