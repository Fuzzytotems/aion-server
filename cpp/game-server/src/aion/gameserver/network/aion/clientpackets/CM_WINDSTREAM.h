#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The client enters, rides, boosts in and leaves a windstream (C_WIND_PATH).
 */
class CM_WINDSTREAM : public AionClientPacket {
private:
	int32_t teleportId{};
	int32_t distance{};
	int32_t state{};

public:
	CM_WINDSTREAM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
