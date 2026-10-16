#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A house object used (C_USE_HOUSING_OBJECT): its controller opens the dialog or the use of the object.
 * <p>
 * C++ only: `CM_USE_HOUSE_OBJECTTestAccess` (tests/cm_lz) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Rolandas
 */
class CM_USE_HOUSE_OBJECT : public AionClientPacket {
	friend struct CM_USE_HOUSE_OBJECTTestAccess;

private:
	int32_t itemObjectId{};

public:
	CM_USE_HOUSE_OBJECT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
