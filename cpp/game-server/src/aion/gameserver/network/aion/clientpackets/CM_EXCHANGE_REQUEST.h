#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A player asks another for an exchange (C_ASK_XCHG): the refusals (no such player, himself, a dead one, too far, hidden, the other race, trade
 * denied), then the question to the other player, whose answer registers the exchange (ExchangeService) or tells the asker the refusal.
 *
 * @author -Avol-
 */
class CM_EXCHANGE_REQUEST : public AionClientPacket {
public:
	std::optional<int32_t> targetObjectId; // Java: public Integer (never null once read)

	CM_EXCHANGE_REQUEST(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
