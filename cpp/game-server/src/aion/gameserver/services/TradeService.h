#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/tradelist/fwd.h"
#include "aion/gameserver/model/trade/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author ATracer, Rama, Wakizashi, xTz, Neon
 */
class TradeService {
private:
	// Java: private static final TradeListData tradeListData = DataManager.TRADE_LIST_DATA and GoodsListData goodsListData =
	// DataManager.GOODSLIST_DATA. Not C++ statics (they would be copied before the static data loads): the bodies read DataManager's holders.
	static bool canBuyLimitItem(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, model::trade::TradeItem& tradeItem);
public:
	static bool performBuyFromShop(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, model::trade::TradeList& tradeList);
	/** General Trade with NPC method. Handles buy items for AP and/or tokens (coins etc.) and/or kinah */
	static bool performBuyTransaction(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, model::trade::TradeList& tradeList,
		bool useKinah);
private:
	static bool validateBuyItems(model::gameobjects::Npc& npc, model::trade::TradeList& tradeList, model::gameobjects::player::Player& player);
public:
	static bool performSellToShop(model::gameobjects::player::Player& player, model::trade::TradeList& tradeList,
		const model::templates::tradelist::TradeListTemplate* purchaseTemplate);
	static bool performSellToShop(model::gameobjects::player::Player& player, model::trade::TradeList& tradeList,
		const model::templates::tradelist::TradeListTemplate* purchaseTemplate, int32_t sellModifier);
	static bool performSellForAPToShop(model::gameobjects::player::Player& player, model::trade::TradeList& tradeList,
		const model::templates::tradelist::TradeListTemplate* purchaseTemplate);
	static bool performBuyFromTradeInTrade(model::gameobjects::player::Player& player, int32_t npcObjectId, int32_t itemId, int32_t count,
		const std::vector<int32_t>& tradeInItemObjectIds);
};

} // namespace aion::gameserver::services
