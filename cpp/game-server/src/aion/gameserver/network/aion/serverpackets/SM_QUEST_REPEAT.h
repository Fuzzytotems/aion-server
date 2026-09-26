#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Rolandas, Neon
 */
class SM_QUEST_REPEAT : public AionServerPacket {
public:
	std::vector<int32_t> repeatableQuests{};
	explicit SM_QUEST_REPEAT(const std::vector<int32_t>& repeatableQuests);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
