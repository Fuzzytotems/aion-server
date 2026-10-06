#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Offers a quest to the online group or alliance group members within range (C_SHARE_QUEST).
 *
 * @author ginho1, Neon
 */
class CM_QUEST_SHARE : public AionClientPacket {
private:
	int32_t questId{};

public:
	CM_QUEST_SHARE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
