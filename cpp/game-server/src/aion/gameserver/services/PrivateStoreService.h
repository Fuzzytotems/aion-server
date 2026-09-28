#pragma once

#include <span>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/trade/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author Simple
 */
class PrivateStoreService {
public:
	static void createStoreWithItems(model::gameobjects::player::Player& player, std::span<const runtime::Ptr<model::trade::TradePSItem>> tradePSItems);
private:
	static bool canOpenPrivateStore(model::gameobjects::player::Player& player);
	static bool validateItem(model::gameobjects::player::PrivateStore& store, runtime::Ptr<model::gameobjects::Item> item,
		model::trade::TradePSItem& psItem);
public:
	static void closePrivateStore(model::gameobjects::player::Player& player);
	/** This method will move the item to the new player and move kinah to item owner */
	static void sellStoreItem(model::gameobjects::player::Player& seller, model::gameobjects::player::Player& buyer,
		model::trade::TradeList& tradeList);
private:
	/** Decrease item count and update inventory */
	static void decreaseItemFromPlayer(model::gameobjects::player::Player& seller, model::gameobjects::Item& item,
		model::trade::TradePSItem& boughtItem);
	static std::vector<runtime::Ref<model::trade::TradePSItem>> getBoughtItems(model::gameobjects::player::Player& seller,
		model::trade::TradeList& tradeList);
public:
	static void openPrivateStore(model::gameobjects::player::Player& activePlayer, std::string_view name);
};

} // namespace aion::gameserver::services
