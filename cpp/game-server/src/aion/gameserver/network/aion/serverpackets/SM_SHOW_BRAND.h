#pragma once

#include <cstdint>
#include <map>
#include <unordered_map>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Sweetkr
 */
class SM_SHOW_BRAND : public AionServerPacket {
private:
	std::map<int32_t, int32_t> targetIdsByIconId{}; // fieldmap.toml: Java HashMap of the brand ids 0..15, which Java iterates in key order
public:
	SM_SHOW_BRAND(int32_t iconId, int32_t targetObjectId);
	explicit SM_SHOW_BRAND(const std::unordered_map<int32_t, int32_t>& targetIdsByIconId);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
