#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * @author Sweetkr
 */
class CM_CUSTOM_SETTINGS : public AionClientPacket {
private:
	int32_t display{};
	int32_t deny{};

public:
	CM_CUSTOM_SETTINGS(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
