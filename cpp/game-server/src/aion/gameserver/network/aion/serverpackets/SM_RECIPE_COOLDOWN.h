#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * zzsort, Sykra
 */
class SM_RECIPE_COOLDOWN : public AionServerPacket {
private:
	int32_t mode{}; // Java: = 0
	std::unordered_map<int32_t, int32_t> cooldowns{}; // Java: = new HashMap<>()
public:
	SM_RECIPE_COOLDOWN(model::gameobjects::player::Player& player, int32_t mode);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
