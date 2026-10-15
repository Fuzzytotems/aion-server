#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player locks his side of the exchange window (C_CHECK_XCHG); the body is empty.
 *
 * @author -Avol-
 */
class CM_EXCHANGE_LOCK : public AionClientPacket {
public:
	CM_EXCHANGE_LOCK(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
