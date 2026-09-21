#include "aion/gameserver/services/reward/StarterKitService.h"

#include "aion/gameserver/model/templates/rewards/RewardItem.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::reward {

namespace {

using RewardItem = model::templates::rewards::RewardItem;
using ItemList = runtime::RcArrayList<runtime::Ref<RewardItem>>;

} // namespace

StarterKitService& StarterKitService::getInstance() {
	static StarterKitService instance; // Java SingletonHolder
	return instance;
}

StarterKitService::StarterKitService() {
	itemMap.put(1, ItemList::create());
	itemMap.put(20, ItemList::create());
	itemMap.put(25, ItemList::create());
	itemMap.put(35, ItemList::create());
	itemMap.put(50, ItemList::create());
	itemMap.put(60, ItemList::create());

	itemMap.get(1)->add(RewardItem::create(169610056, 1));    // [Title Card] Novice of Atreia - 30-day pass
	itemMap.get(20)->add(RewardItem::create(188054100, 1));   // Bronze Coin Box
	itemMap.get(20)->add(RewardItem::create(125001832, 1));   // Experienced Lepharist Veil
	itemMap.get(20)->add(RewardItem::create(122000449, 1));   // Ghost Rose Quartz Ring
	itemMap.get(20)->add(RewardItem::create(122000451, 1));   // Ghost Crystal Ring
	itemMap.get(20)->add(RewardItem::create(120015052, 1));   // Prestigious Magic Earrings
	itemMap.get(20)->add(RewardItem::create(120015051, 1));   // Prestigious Combat Earrings
	itemMap.get(20)->add(RewardItem::create(123000879, 1));   // Morai's Belt
	itemMap.get(25)->add(RewardItem::create(190100032, 1));   // Pagati Ironhide
	itemMap.get(25)->add(RewardItem::create(164002272, 25));  // [Event] Enduring Greater Raging Wind Scroll
	itemMap.get(25)->add(RewardItem::create(162000039, 25));  // Divine Wind Serum
	itemMap.get(25)->add(RewardItem::create(162002018, 25));  // [Event] Wormwood Dish
	itemMap.get(35)->add(RewardItem::create(188054101, 1));   // Silver Coin Box
	itemMap.get(35)->add(RewardItem::create(169620082, 1));   // Gathering Boost Charm II - 100%
	itemMap.get(35)->add(RewardItem::create(169620094, 1));   // Crafting Boost Charm III - 100%
	itemMap.get(50)->add(RewardItem::create(121000815, 1));   // Lonely Diamond Necklace
	itemMap.get(50)->add(RewardItem::create(120000901, 1));   // Lonely Diamond Earrings
	itemMap.get(50)->add(RewardItem::create(122001038, 1));   // Lonely Diamond Ring
	itemMap.get(50)->add(RewardItem::create(188053624, 10));  // Return Scroll Bundle
	itemMap.get(50)->add(RewardItem::create(161001001, 5));   // Revival Stone
	itemMap.get(60)->add(RewardItem::create(169620072, 1));   // AP Boost Charm II - 30%
	itemMap.get(60)->add(RewardItem::create(162002030, 100)); // Event] Premium Restoration Serum
	itemMap.get(60)->add(RewardItem::create(162002018, 50));  // [Event] Wormwood Dish
	itemMap.get(60)->add(RewardItem::create(188053526, 5));   // [Event] Aion's Steel Form Candy Box
	itemMap.get(60)->add(RewardItem::create(188053783, 5));   // Stigma Sack
}

void StarterKitService::onLevelUp(model::gameobjects::player::Player& player, int32_t fromLevel, int32_t toLevel) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::reward
