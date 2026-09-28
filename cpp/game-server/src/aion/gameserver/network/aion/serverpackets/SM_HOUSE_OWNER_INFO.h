#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Rolandas, Neon
 */
class SM_HOUSE_OWNER_INFO : public AionServerPacket {
private:
	int32_t playerHouseOwnerState{};
	runtime::Ref<model::house::House> activeHouse{};
	runtime::Ref<model::house::House> inactiveHouse{};

public:
	explicit SM_HOUSE_OWNER_INFO(model::gameobjects::player::Player& player);
	~SM_HOUSE_OWNER_INFO() override;

protected:
	void writeImpl(AionConnection* con) override;

private:
	int32_t calculateWeeksUntilNextPay();
};

} // namespace aion::gameserver::network::aion::serverpackets
