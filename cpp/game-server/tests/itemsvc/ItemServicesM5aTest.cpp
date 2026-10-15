// P5-07 item services, M5a subset (m5a-plan.md E1-01): ItemFactory, RepurchaseService, the limited item model, LimitedItemTradeService.start,
// WarehouseService.sendWarehouseInfo and StigmaService.onPlayerLogin for a fresh character.
//
// Expectations are derived by hand from ItemFactory.java:18-38, RepurchaseService.java:27-45, LimitedItem.java, LimitedTradeNpc.java,
// LimitedItemTradeService.java:29-66, WarehouseService.java:124-157 and StigmaService.java:88-132.

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <unordered_set>
#include <vector>

#include "../playersvc/PlayerEventsTestSupport.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GoodsListData.bind.h"
#include "aion/gameserver/dataholders/GoodsListData.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/TradeListData.bind.h"
#include "aion/gameserver/dataholders/TradeListData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/storage/IStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/limiteditems/LimitedItem.h"
#include "aion/gameserver/model/limiteditems/LimitedTradeNpc.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/LimitedItemTradeService.h"
#include "aion/gameserver/services/RepurchaseService.h"
#include "aion/gameserver/services/StigmaService.h"
#include "aion/gameserver/services/WarehouseService.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/item/ItemFactory.h"

namespace aion::gameserver::playerevents::test {
namespace {

using model::gameobjects::Item;
using model::limiteditems::LimitedItem;
using model::limiteditems::LimitedTradeNpc;
using runtime::Ptr;
using runtime::Ref;

class ItemServicesM5aTest : public PlayerEventsTest {};

TEST_F(ItemServicesM5aTest, ItemFactoryCreatesItemsAndCapsTheCountAtTheStackSizeExceptForKinah) {
	PublishedHolder itemData(dataholders::DataManager::ITEM_DATA,
		bindXml<dataholders::ItemData>(R"(<item_templates><item_template id="100000001" item_group="SWORD"/>)"
									   R"(<item_template id="182400001" max_stack_count="1000"/>)"
									   R"(<item_template id="169000001" max_stack_count="100"/></item_templates>)"));
	// a kinah cap makes the kinah template's max stack count small: Java still keeps the requested kinah count (ItemFactory.java:35)
	AtomicConfigScope<bool> kinahCap(configs::main::CustomConfig::ENABLE_KINAH_CAP, true);
	AtomicConfigScope<int64_t> kinahCapValue(configs::main::CustomConfig::KINAH_CAP_VALUE, 50);

	EXPECT_FALSE(services::item::ItemFactory::newItem(123)) << "no template: null after the error log";

	Ref<Item> sword;
	Ref<Item> stack;
	Ref<Item> kinah;
	SKIP_IF_UNPORTED({
		sword = services::item::ItemFactory::newItem(100000001);
		stack = services::item::ItemFactory::newItem(169000001, 250);
		kinah = services::item::ItemFactory::newItem(182400001, 5000);
	});
	ASSERT_TRUE(sword && stack && kinah);
	EXPECT_EQ(sword->getItemTemplate(), dataholders::DataManager::ITEM_DATA->getItemTemplate(100000001));
	EXPECT_EQ(stack->getItemCount(), 100) << "capped at max_stack_count";
	EXPECT_EQ(kinah->getItemCount(), 5000) << "kinah is never capped";
	EXPECT_NE(sword->getObjectId(), stack->getObjectId()) << "IDFactory ids";

	Ref<Item> small;
	SKIP_IF_UNPORTED(small = services::item::ItemFactory::newItem(169000001, 7));
	EXPECT_EQ(small->getItemCount(), 7);

	// Java: newItem(id, count) dereferences the null item of an unknown template
	EXPECT_THROW(services::item::ItemFactory::newItem(123, 1), runtime::NullPointerException);
}

TEST_F(ItemServicesM5aTest, RepurchaseItemsArePerPlayerAndRemovedOnLogout) {
	PublishedHolder itemData(dataholders::DataManager::ITEM_DATA,
		bindXml<dataholders::ItemData>(R"(<item_templates><item_template id="169000001" max_stack_count="100"/></item_templates>)"));
	PlayerFixture f = makePlayer(1, 100);
	Ref<Item> first;
	Ref<Item> second;
	SKIP_IF_UNPORTED({
		first = Item::create(500, dataholders::DataManager::ITEM_DATA->getItemTemplate(169000001));
		second = Item::create(501, dataholders::DataManager::ITEM_DATA->getItemTemplate(169000001));
	});
	services::RepurchaseService& service = services::RepurchaseService::getInstance();
	EXPECT_TRUE(service.getRepurchaseItems(1).empty()) << "Java: Collections.emptySet()";
	EXPECT_FALSE(service.canRepurchase(*f.player, 500));

	service.addRepurchaseItems(*f.player, {Ptr<Item>(first), Ptr<Item>(second)});
	EXPECT_EQ(service.getRepurchaseItems(1).size(), 2u);
	EXPECT_TRUE(service.canRepurchase(*f.player, 501));
	EXPECT_FALSE(service.canRepurchase(*f.player, 502));
	EXPECT_TRUE(service.getRepurchaseItems(2).empty()) << "other players see nothing";

	service.removeRepurchaseItems(*f.player);
	EXPECT_TRUE(service.getRepurchaseItems(1).empty());
	service.removeRepurchaseItems(*f.player); // Java: removing an absent entry is a no-op
}

TEST_F(ItemServicesM5aTest, LimitedItemBuyCountsAndReset) {
	Ref<LimitedItem> item = LimitedItem::create(169000001, 10, 3, "0 0 0 ? * *");
	EXPECT_EQ(item->getBuyCount(7), 0) << "Java getOrDefault(playerObjectId, 0)";
	item->setBuyCount(7, 2);
	item->setBuyCount(8, 1);
	item->setSellLimit(4);
	EXPECT_EQ(item->getBuyCount(7), 2);

	item->setToDefault();
	EXPECT_EQ(item->getSellLimit(), 10) << "back to the default sell limit";
	EXPECT_EQ(item->getBuyCount(7), 0);
	EXPECT_EQ(item->getBuyCount(8), 0);

	Ref<LimitedTradeNpc> npc = LimitedTradeNpc::create();
	Ref<LimitedItem> other = LimitedItem::create(169000002, 5, 0, "0 0 0 ? * *");
	npc->addLimitedItems({Ptr<LimitedItem>(item)});
	npc->addLimitedItems({Ptr<LimitedItem>(other), Ptr<LimitedItem>(item)});
	std::vector<Ptr<LimitedItem>> items = npc->getLimitedItems().snapshot();
	ASSERT_EQ(items.size(), 3u) << "Java ArrayList.addAll keeps duplicates and order";
	EXPECT_EQ(items[0].get(), item.get());
	EXPECT_EQ(items[1].get(), other.get());
}

TEST_F(ItemServicesM5aTest, LimitedItemTradeServiceStartSchedulesTheLimitedItemsOfTradeListNpcs) {
	services::cron::CronService::initSingleton(std::make_unique<services::cron::CurrentThreadRunnableRunner>(), std::chrono::locate_zone("UTC"),
		services::cron::CronService::Driver::EXECUTOR);
	struct CronReset {
		~CronReset() { services::cron::CronService::resetForTests(); }
	} cronReset;
	// npc 800 sells goods list 1 (one limited item) and 2 (none); npc 801 names the missing goods list 9 (warning) and goods list 1 again
	PublishedHolder goods(dataholders::DataManager::GOODSLIST_DATA,
		bindXml<dataholders::GoodsListData>(R"(<goodslists><list id="1"><salestime>0 0 0 ? * *</salestime><item id="169000001" buy_limit="2" sell_limit="5"/>)"
											R"(<item id="169000002"/></list><list id="2"><item id="169000003"/></list></goodslists>)"));
	PublishedHolder tradeLists(dataholders::DataManager::TRADE_LIST_DATA,
		bindXml<dataholders::TradeListData>(R"(<npc_trade_list><tradelist_template npc_id="800"><tradelist id="1"/><tradelist id="2"/></tradelist_template>)"
											R"(<tradelist_template npc_id="801"><tradelist id="9"/><tradelist id="1"/></tradelist_template></npc_trade_list>)"));

	services::LimitedItemTradeService& service = services::LimitedItemTradeService::getInstance();
	service.start();

	EXPECT_TRUE(service.isLimitedTradeNpc(800));
	EXPECT_TRUE(service.isLimitedTradeNpc(801));
	EXPECT_FALSE(service.isLimitedTradeNpc(802));
	Ptr<LimitedItem> limited = service.getLimitedItem(169000001, 800);
	ASSERT_TRUE(limited);
	EXPECT_EQ(limited->getBuyLimit(), 2);
	EXPECT_EQ(limited->getSellLimit(), 5);
	EXPECT_FALSE(service.getLimitedItem(169000002, 800)) << "items without buy and sell limits are not limited";
	EXPECT_FALSE(service.getLimitedItem(169000001, 802));
	ASSERT_TRUE(service.getLimitedTradeNpc(801));
	EXPECT_EQ(service.getLimitedTradeNpc(801)->getLimitedItems().size(), 1);
	EXPECT_EQ(services::cron::CronService::getInstance().getJobCount(), 2u) << "one cron job per limited item of each npc";
}

TEST_F(ItemServicesM5aTest, FreshCharacterLoginPathsReachNoUnportedBody) {
	PlayerFixture f = makePlayer(3, 300);
	runtime::resetUnportedHitsForTests();
	// Java: the first enter world sends the regular warehouse closing part and both account warehouse parts (empty storages)
	services::WarehouseService::sendWarehouseInfo(*f.player, true);
	services::WarehouseService::sendWarehouseInfo(*f.player, false);
	// no stigma autolearn permission, no equipped stigmas: nothing is unequipped or learned
	AtomicConfigScope<int8_t> autolearn(configs::main::MembershipConfig::STIGMA_AUTOLEARN, 10);
	services::StigmaService::onPlayerLogin(*f.player);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}


TEST_F(ItemServicesM5aTest, WarehouseInfoIsSentForTheEmptyAndForTheChunkedRegularWarehouse) {
	using model::items::storage::StorageType;
	PublishedHolder itemData(dataholders::DataManager::ITEM_DATA,
		bindXml<dataholders::ItemData>(R"(<item_templates><item_template id="169000001" max_stack_count="100"/></item_templates>)"));
	PlayerFixture f = makePlayer(4, 400);
	const int32_t regularWarehouse = model::items::storage::getId(StorageType::REGULAR_WAREHOUSE);
	Ptr<model::items::storage::IStorage> warehouse = f.player->getStorage(regularWarehouse);
	ASSERT_TRUE(warehouse);
	ASSERT_TRUE(warehouse->getItems().empty()) << "a fresh character (plan D1)";
	runtime::resetUnportedHitsForTests();

	// Java (WarehouseService.java:131-157) for an empty regular warehouse: the closing regular part, then the account parts
	services::WarehouseService::sendWarehouseInfo(*f.player, true);
	services::WarehouseService::sendWarehouseInfo(*f.player, false);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
	EXPECT_EQ(f.player->getWarehouseExpansions(), 0) << "the expand level written into every regular part";

	// 11 items make the `while (index + 10 < itemsSize)` loop run once (10 items, firstPacket true), then the rest (1 item, firstPacket false)
	for (int32_t i = 0; i < 11; ++i) {
		Ref<Item> item = Item::create(600 + i, dataholders::DataManager::ITEM_DATA->getItemTemplate(169000001));
		warehouse->onLoadHandler(*item);
	}
	ASSERT_EQ(warehouse->getItems().size(), 11u);
	services::WarehouseService::sendWarehouseInfo(*f.player, true);
	services::WarehouseService::sendWarehouseInfo(*f.player, false);
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "the chunked branch reaches no unported body";
	EXPECT_EQ(warehouse->getItems().size(), 11u) << "sending the info does not change the storage";
}

} // namespace
} // namespace aion::gameserver::playerevents::test
