#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author Simple, Luzien
 */
class WarehouseService {
private:
	static constexpr int32_t MAX_EXPAND = 11;
public:
	/** Shows Question window and expands on positive response */
	static void expandWarehouse(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc);
	static void expand(model::gameobjects::player::Player& player, bool isNpcExpand);
	static bool canExpandByTicket(model::gameobjects::player::Player& player, int32_t ticketLevel);
	static bool canExpand(model::gameobjects::player::Player& player);
private:
	static int32_t getCompletedWhQuests(model::gameobjects::player::Player& player);
public:
	/** Sends correctly warehouse packets */
	static void sendWarehouseInfo(model::gameobjects::player::Player& player, bool sendAccountWh);
};

} // namespace aion::gameserver::services
