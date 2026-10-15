#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author namedrisk
 */
class SM_RECIPE_DELETE : public AionServerPacket {
private:
	int32_t recipeId{};
public:
	explicit SM_RECIPE_DELETE(int32_t recipeId);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
