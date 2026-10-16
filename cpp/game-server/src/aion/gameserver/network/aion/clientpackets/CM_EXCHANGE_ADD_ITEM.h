#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player puts an item of his cube into the exchange window (C_ADD_XCHG).
 *
 * @author Avol
 */
class CM_EXCHANGE_ADD_ITEM : public AionClientPacket {
public:
	int32_t itemObjId{};
	int32_t itemCount{};

	CM_EXCHANGE_ADD_ITEM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
