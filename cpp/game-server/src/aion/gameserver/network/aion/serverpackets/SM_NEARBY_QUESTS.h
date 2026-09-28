#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author MrPoke, Rolandas, Neon
 */
class SM_NEARBY_QUESTS : public AionServerPacket {
private:
	static constexpr int32_t notYetAvailableBit = 1 << 17;
	std::unordered_map<int32_t, int32_t> nearbyQuestList{};
public:
	explicit SM_NEARBY_QUESTS(const std::unordered_map<int32_t, int32_t>& nearbyQuestList);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
