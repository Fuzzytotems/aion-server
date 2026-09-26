#pragma once

#include <cstdint>
#include <unordered_set>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author lord_rex
 */
class SM_RECIPE_LIST : public AionServerPacket {
private:
	std::unordered_set<int32_t> recipeIds{};
public:
	explicit SM_RECIPE_LIST(const std::unordered_set<int32_t>& recipeIds);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
