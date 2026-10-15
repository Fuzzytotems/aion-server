#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The house door state, the owner name display and the sign notice (C_HOUSING_CONFIG).
 * <p>
 * C++ only: `CM_HOUSE_SETTINGSTestAccess` (tests/cm_lz) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Rolandas
 */
class CM_HOUSE_SETTINGS : public AionClientPacket {
	friend struct CM_HOUSE_SETTINGSTestAccess;

private:
	int8_t doorState{};
	bool showOwnerName{};
	std::string signNotice;

public:
	CM_HOUSE_SETTINGS(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
