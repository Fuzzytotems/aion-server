#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A relationship crystal's teleport to the own house, a friend's or a random friend's (C_REQUEST_HOUSING_TELEPORT).
 * <p>
 * C++ only: `CM_HOUSE_TELEPORTTestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Rolandas, Neon
 */
class CM_HOUSE_TELEPORT : public AionClientPacket {
	friend struct CM_HOUSE_TELEPORTTestAccess;

private:
	int32_t actionId{};
	int32_t playerId1{};
	int32_t playerId2{};

	std::vector<runtime::Ptr<model::house::House>> findFriendsAccessibleHouses(model::gameobjects::player::Player& player);
	void addHouseIfAccessible(model::gameobjects::player::Player& player, std::vector<runtime::Ptr<model::house::House>>& relationIds, int32_t friendId);

public:
	CM_HOUSE_TELEPORT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
