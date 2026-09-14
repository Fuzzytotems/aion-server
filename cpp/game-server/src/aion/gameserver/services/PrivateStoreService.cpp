#include "aion/gameserver/services/PrivateStoreService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("EXCHANGE_LOG");

void PrivateStoreService::createStoreWithItems(model::gameobjects::player::Player& player,
	std::span<const runtime::Ptr<model::trade::TradePSItem>> tradePSItems) {
	AION_UNPORTED();
}

bool PrivateStoreService::canOpenPrivateStore(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool PrivateStoreService::validateItem(model::gameobjects::player::PrivateStore& store, runtime::Ptr<model::gameobjects::Item> item,
	model::trade::TradePSItem& psItem) {
	AION_UNPORTED();
}

void PrivateStoreService::closePrivateStore(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PrivateStoreService::sellStoreItem(model::gameobjects::player::Player& seller, model::gameobjects::player::Player& buyer,
	model::trade::TradeList& tradeList) {
	AION_UNPORTED();
}

void PrivateStoreService::decreaseItemFromPlayer(model::gameobjects::player::Player& seller, model::gameobjects::Item& item,
	model::trade::TradePSItem& boughtItem) {
	AION_UNPORTED();
}

std::vector<runtime::Ref<model::trade::TradePSItem>> PrivateStoreService::getBoughtItems(model::gameobjects::player::Player& seller,
	model::trade::TradeList& tradeList) {
	AION_UNPORTED();
}

void PrivateStoreService::openPrivateStore(model::gameobjects::player::Player& activePlayer, std::string_view name) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
