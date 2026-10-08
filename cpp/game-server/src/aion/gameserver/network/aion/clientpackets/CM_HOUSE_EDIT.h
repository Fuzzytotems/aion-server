#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The house decoration mode (C_HOUSING_EDIT): enter and leave it, register an item, delete, place, move and take back a house object, the renovation mode and a building change.
 * <p>
 * C++ only: `CM_HOUSE_EDITTestAccess` (tests/cm_lz) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Rolandas
 */
class CM_HOUSE_EDIT : public AionClientPacket {
	friend struct CM_HOUSE_EDITTestAccess;

private:
	int32_t action{};
	int32_t itemObjectId{};
	float x{}, y{}, z{};
	int32_t rotation{};
	int32_t buildingId{};

public:
	CM_HOUSE_EDIT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;

private:
	bool removeRenovationCoupon(model::gameobjects::player::Player& player, model::house::House& house);
};

} // namespace aion::gameserver::network::aion::clientpackets
