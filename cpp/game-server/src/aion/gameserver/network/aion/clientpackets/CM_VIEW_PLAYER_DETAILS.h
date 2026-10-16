#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player inspects another player's equipment (C_VIEW_OTHER_INVENTORY).
 *
 * @author Avol
 */
class CM_VIEW_PLAYER_DETAILS : public AionClientPacket {
private:
	int32_t targetObjectId{};

public:
	CM_VIEW_PLAYER_DETAILS(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
