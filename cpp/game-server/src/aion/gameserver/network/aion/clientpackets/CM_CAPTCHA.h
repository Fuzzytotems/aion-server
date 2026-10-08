#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Received when a player answers the gather captcha or asks for his restriction (/ExtractStatus).
 *
 * @author Cura
 */
class CM_CAPTCHA : public AionClientPacket {
private:
	int32_t type = 0;
	int32_t count = 0;
	std::string word;

public:
	CM_CAPTCHA(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
