#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player confirms the locked exchange (C_ACCEPT_XCHG); the body is empty.
 *
 * @author -Avol-
 */
class CM_EXCHANGE_OK : public AionClientPacket {
public:
	CM_EXCHANGE_OK(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
