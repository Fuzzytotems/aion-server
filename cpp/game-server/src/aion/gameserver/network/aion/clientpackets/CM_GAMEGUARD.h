#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/** The client's game guard data (its size is checked against the expected aion.bin size). */
class CM_GAMEGUARD : public AionClientPacket {
private:
	int32_t size{};

public:
	CM_GAMEGUARD(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
