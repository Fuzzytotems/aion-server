#include "aion/gameserver/services/TradeService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.TradeService");

bool TradeService::canBuyLimitItem(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, model::trade::TradeItem& tradeItem) {
	AION_UNPORTED();
}

bool TradeService::performBuyFromShop(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, model::trade::TradeList& tradeList) {
	AION_UNPORTED();
}

bool TradeService::performBuyTransaction(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, model::trade::TradeList& tradeList,
	bool useKinah) {
	AION_UNPORTED();
}

bool TradeService::validateBuyItems(model::gameobjects::Npc& npc, model::trade::TradeList& tradeList, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool TradeService::performSellToShop(model::gameobjects::player::Player& player, model::trade::TradeList& tradeList,
	const model::templates::tradelist::TradeListTemplate* purchaseTemplate) {
	AION_UNPORTED();
}

bool TradeService::performSellToShop(model::gameobjects::player::Player& player, model::trade::TradeList& tradeList,
	const model::templates::tradelist::TradeListTemplate* purchaseTemplate, int32_t sellModifier) {
	AION_UNPORTED();
}

bool TradeService::performSellForAPToShop(model::gameobjects::player::Player& player, model::trade::TradeList& tradeList,
	const model::templates::tradelist::TradeListTemplate* purchaseTemplate) {
	AION_UNPORTED();
}

bool TradeService::performBuyFromTradeInTrade(model::gameobjects::player::Player& player, int32_t npcObjectId, int32_t itemId, int32_t count,
	const std::vector<int32_t>& tradeInItemObjectIds) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
