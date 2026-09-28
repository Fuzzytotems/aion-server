#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * @author -Nemesiss-, Neon
 */
class CM_QUIT : public AionClientPacket {
private:
	/** if true, player wants to go to the character selection or plastic surgery screen. */
	bool stayConnected{};

public:
	CM_QUIT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
