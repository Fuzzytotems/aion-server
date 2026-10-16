#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author lord_rex
 */
class SM_LEARN_RECIPE : public AionServerPacket {
private:
	int32_t recipeId{};
public:
	explicit SM_LEARN_RECIPE(int32_t recipeId);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
