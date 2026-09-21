#include "aion/gameserver/services/LimitedItemTradeService.h"

#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GoodsListData.h"
#include "aion/gameserver/dataholders/TradeListData.h"
#include "aion/gameserver/model/limiteditems/LimitedItem.h"
#include "aion/gameserver/model/limiteditems/LimitedTradeNpc.h"
#include "aion/gameserver/model/templates/goods/GoodsList.h"
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"
#include "aion/gameserver/services/cron/CronService.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.LimitedItemTradeService");

LimitedItemTradeService::LimitedItemTradeService() = default;

LimitedItemTradeService::~LimitedItemTradeService() = default;

LimitedItemTradeService& LimitedItemTradeService::getInstance() {
	static LimitedItemTradeService instance; // Java SingletonHolder
	return instance;
}

// callback at LimitedItemTradeService.java:46 (fieldmap key LimitedItemTradeService@L46:40)
void LimitedItemTradeService::start() {
	using model::limiteditems::LimitedItem;
	using model::limiteditems::LimitedTradeNpc;
	for (const auto& [npcId, npc] : dataholders::DataManager::TRADE_LIST_DATA->getTradeListTemplate()) {
		for (const model::templates::tradelist::TradeListTemplate::TradeTab& list : npc->getTradeTablist()) {
			const model::templates::goods::GoodsList* goodsList = dataholders::DataManager::GOODSLIST_DATA->getGoodsListById(list.getId());
			if (goodsList == nullptr) {
				log.warn("No goodslist for tradelist of npc " + std::to_string(npcId));
				continue;
			}
			std::vector<runtime::Ref<LimitedItem>> limitedItems = goodsList->getLimitedItems();
			if (!limitedItems.empty()) {
				std::vector<runtime::Ptr<LimitedItem>> items(limitedItems.begin(), limitedItems.end());
				runtime::Ptr<LimitedTradeNpc> limitedTradeNpc = limitedTradeNpcs.computeIfAbsent(npcId, [] { return LimitedTradeNpc::create(); });
				limitedTradeNpc->addLimitedItems(items);
			}
		}
	}

	for (const runtime::Ptr<LimitedTradeNpc>& limitedTradeNpc : limitedTradeNpcs.values()) {
		for (const runtime::Ptr<LimitedItem>& limitedItem : limitedTradeNpc->getLimitedItems().snapshot()) {
			// Java: limitedItem::setToDefault
			cron::CronService::getInstance().schedule(
				runtime::bindTask([](LimitedItem& item) { item.setToDefault(); }, runtime::Ref<LimitedItem>(limitedItem)),
				limitedItem->getSalesTime());
		}
	}
	log.info("Scheduled Limited Items based on cron expression size: " + std::to_string(limitedTradeNpcs.size()));
}

runtime::Ptr<model::limiteditems::LimitedItem> LimitedItemTradeService::getLimitedItem(int32_t itemId, int32_t npcId) {
	if (limitedTradeNpcs.containsKey(npcId)) {
		for (const runtime::Ptr<model::limiteditems::LimitedItem>& limitedItem : limitedTradeNpcs.get(npcId)->getLimitedItems().snapshot()) {
			if (limitedItem->getItemId() == itemId) {
				return limitedItem;
			}
		}
	}
	return nullptr;
}

bool LimitedItemTradeService::isLimitedTradeNpc(int32_t npcId) {
	return limitedTradeNpcs.containsKey(npcId);
}

runtime::Ptr<model::limiteditems::LimitedTradeNpc> LimitedItemTradeService::getLimitedTradeNpc(int32_t npcId) {
	return limitedTradeNpcs.get(npcId);
}

} // namespace aion::gameserver::services
