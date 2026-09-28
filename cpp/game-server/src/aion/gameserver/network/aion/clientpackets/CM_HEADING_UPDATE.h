#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Client sends this packet after spin effect to update the heading (in that case we already set the heading before we receive this packet).
 */
class CM_HEADING_UPDATE : public AionClientPacket {
public:
	// TODO: Find out when else this packet is sent and what or even if we have to answer
	CM_HEADING_UPDATE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
