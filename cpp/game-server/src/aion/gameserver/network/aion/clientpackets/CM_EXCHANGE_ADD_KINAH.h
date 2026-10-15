#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player puts kinah into the exchange window (C_XCHG_GOLD).
 * <p>
 * C++ only: `CM_EXCHANGE_ADD_KINAHTestAccess` (tests/cm_ak/ExchangePacketsTest.cpp) reads the field readImpl decoded, which Java keeps private
 * without a getter.
 *
 * @author Avol
 */
class CM_EXCHANGE_ADD_KINAH : public AionClientPacket {
	friend struct CM_EXCHANGE_ADD_KINAHTestAccess;

private:
	int64_t kinahCount{};

public:
	CM_EXCHANGE_ADD_KINAH(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
