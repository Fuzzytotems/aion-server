#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The client releases a charge skill it is charging (C_FIRE_CHARGE_SKILL): the server fires the charged skill whose charge time was reached.
 *
 * @author Cheatkiller
 */
class CM_USE_CHARGE_SKILL : public AionClientPacket {
public:
	CM_USE_CHARGE_SKILL(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
