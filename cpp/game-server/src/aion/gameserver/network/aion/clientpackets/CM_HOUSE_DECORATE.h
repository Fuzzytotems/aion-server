#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A wall, floor or other decoration applied to a room, or the default one restored (C_HOUSING_CHANGE_ROOM_DECO).
 * <p>
 * C++ only: `CM_HOUSE_DECORATETestAccess` (tests/cm_lz) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Rolandas
 */
class CM_HOUSE_DECORATE : public AionClientPacket {
	friend struct CM_HOUSE_DECORATETestAccess;

private:
	int32_t objectId{};
	int32_t lineNo{}; // Line number (starts from 1 in 3.0 and from 2 in 3.5) of part in House render/update packet

public:
	CM_HOUSE_DECORATE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
