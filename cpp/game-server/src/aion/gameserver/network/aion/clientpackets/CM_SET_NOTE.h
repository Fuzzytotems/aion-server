#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Received when a player sets his note (C_TODAY_WORDS).
 *
 * @author Ben
 */
class CM_SET_NOTE : public AionClientPacket {
private:
	std::string note;

public:
	CM_SET_NOTE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
