#include "aion/gameserver/services/reward/AdventService.h"

#include <chrono>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/dao/AdventDAO.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::services::reward {

using model::templates::rewards::RewardItem;

namespace {

/** Java: ServerTime.now().toLocalDate() */
commons::database::Date serverToday() {
	return commons::database::Date(std::chrono::floor<std::chrono::days>(utils::time::ServerTime::now().get_local_time()));
}

/** Java: DataManager.ITEM_DATA.getItemTemplate(id) dereferenced - an unknown id is a NullPointerException */
const model::templates::item::ItemTemplate& templateOf(int32_t itemId) {
	const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
	if (itemTemplate == nullptr)
		throw runtime::NullPointerException("ItemData.getItemTemplate(" + std::to_string(itemId) + ")");
	return *itemTemplate;
}

} // namespace

AdventService::AdventService() {
	addReward(1, 170190034, 1); // [Event] Solorius Cake
	addReward(2, 125040166, 1); // Solorius Hairpin/Top Hat
	addReward(3, 186000237, 1000); // Ancient Coin
	addReward(4, 188051879, 1); // Solorius Furniture Set Box
	addReward(5, 166100011, 600); // Greater Supplements (Mythic)
	addReward(6, 190020236, 1); // Mini Hyperion Egg (30 days)
	addReward(7, 188051297, 1); // 12 Solorius Inquin Form Candy (Elyos)
	addReward(7, 188051298, 1); // 12 Solorius Inquin Form Candy (Asmodians)
	addReward(8, 166020003, 10); // [Event] Omega Enchantment Stone
	addReward(9, 170390016, 1); // Solorius Garden Tree (Elyos)
	addReward(9, 170395016, 1); // Solorius Garden Tree (Asmodians)
	addReward(10, 160010201, 25); // [Event] Solorius Cookie
	addReward(11, 162001057, 5); // Tea of Repose - 100% Recovery
	addReward(12, 188050004, 5); // Red Solorius Stocking (Elyos)
	addReward(12, 188050007, 5); // Red Solorius Stocking (Asmodians)
	addReward(13, 110900665, 1); // Resplendent Jolly Coat
	addReward(14, 166030007, 5); // [Event] Tempering Solution
	addReward(15, 188054014, 5); // [Event] Lunahare Kisk Box
	addReward(16, 164002167, 25); // [Event] Drana Coffee
	addReward(17, 188051879, 1); // Solorius Furniture Set Box
	addReward(18, 188051299, 1); // 12 Solorius Tiger Form Candy (Elyos)
	addReward(18, 188051300, 1); // 12 Solorius Tiger Form Candy (Asmodians)
	addReward(19, 186000143, 250); // Kahrun's Symbol
	addReward(20, 162002018, 20); // [Event] Wormwood Dish
	addReward(21, 188050006, 5); // Green Solorius Stocking (Elyos)
	addReward(21, 188050009, 5); // Green Solorius Stocking (Asmodians)
	addReward(22, 166500005, 5); // [Event] Amplification Stone
	addReward(23, 160010201, 25); // [Event] Solorius Cookie
	addReward(24, 190020109, 1); // Solorinerk Egg
	addReward(24, 188053610, 5); // [Event] Level 70 Composite Manastone Bundle
	addReward(24, 166150019, 5); // Assured Greater Felicitous Socketing (Mythic)
}

AdventService::~AdventService() = default;

AdventService& AdventService::getInstance() {
	static AdventService instance; // Java SingletonHolder
	return instance;
}

void AdventService::addReward(int32_t day, int32_t itemId, int64_t itemCount) {
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<RewardItem>>> dayRewards =
		rewards.computeIfAbsent(day, [] { return runtime::RcArrayList<runtime::Ref<RewardItem>>::create(AION_LOCK_CLASS(AdventService::rewards)); });
	dayRewards->add(RewardItem::create(itemId, itemCount));
}

void AdventService::onLogin(model::gameobjects::player::Player& player) {
	if (!configs::main::EventsConfig::ENABLE_ADVENT_CALENDAR.load())
		return;
	commons::database::Date today = serverToday();
	if (!isAdventSeason(today))
		return;
	if (!utils::chathandlers::ChatProcessor::getInstance().isCommandAllowed(player, "advent"))
		return;
	int32_t day = static_cast<int32_t>(static_cast<unsigned>(today.day()));
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<RewardItem>>> dayRewards = rewards.get(day);
	if (!rewards.containsKey(day) || dayRewards->isEmpty() || !dao::AdventDAO::canReceiveReward(player, today))
		return;
	utils::PacketSendUtility::sendMessage(player, "You can open your advent calendar door for today!"
		"\nType in .advent to redeem todays reward on this character.\n" +
		utils::ChatUtil::color("ATTENTION:", utils::JavaColor::PINK) + " Only one character per account can receive this reward!");
}

bool AdventService::isAdventSeason() {
	return isAdventSeason(serverToday());
}

bool AdventService::isAdventSeason(commons::database::Date date) {
	return date.month() == std::chrono::December && static_cast<unsigned>(date.day()) <= 24;
}

// Java AdventService.java:90-125
void AdventService::redeemReward(model::gameobjects::player::Player& player) {
	commons::database::Date today = serverToday();
	int32_t day = static_cast<int32_t>(static_cast<unsigned>(today.day()));
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<RewardItem>>> todaysRewards = rewards.get(day);

	if (!isAdventSeason(today) || todaysRewards == nullptr || todaysRewards->isEmpty()) {
		utils::PacketSendUtility::sendMessage(player, "There is no advent calendar door for today.");
		return;
	}

	if (!dao::AdventDAO::canReceiveReward(player, today)) {
		utils::PacketSendUtility::sendMessage(player, "You have already opened today's advent calendar door on this account.");
		return;
	}

	int64_t regularCubeItems = 0;
	for (runtime::Ptr<RewardItem> r : *todaysRewards) {
		const model::templates::item::ItemTemplate& itemTemplate = templateOf(r->getId());
		if (itemTemplate.getExtraInventoryId() <= 0 && itemTemplate.getRace() != player.getOppositeRace())
			regularCubeItems++;
	}
	if (player.getInventory().getFreeSlots() < regularCubeItems) {
		utils::PacketSendUtility::sendMessage(player, "You don't have enough free slots in your inventory.");
		return;
	}

	if (!dao::AdventDAO::storeLastReceivedDay(player, today)) {
		utils::PacketSendUtility::sendMessage(player, "Sorry. Some shugo broke our database, please report this in our bugtracker :(");
		return;
	}

	for (runtime::Ptr<RewardItem> item : *todaysRewards) {
		if (templateOf(item->getId()).getRace() == player.getOppositeRace())
			continue;
		item::ItemService::addItem(player, item->getId(), item->getCount(), true);
	}
}

// Java AdventService.java:127-144
void AdventService::showTodaysReward(model::gameobjects::player::Player& player) {
	commons::database::Date today = serverToday();
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<RewardItem>>> todaysRewards =
		rewards.get(static_cast<int32_t>(static_cast<unsigned>(today.day())));
	if (today.month() != std::chrono::December || todaysRewards == nullptr || todaysRewards->isEmpty()) {
		utils::PacketSendUtility::sendMessage(player, "There is no advent calendar door for today.");
		return;
	}

	std::string sb("Today's advent calendar reward(s):\n");

	std::vector<runtime::Ptr<RewardItem>> items;
	for (runtime::Ptr<RewardItem> r : *todaysRewards)
		items.push_back(r);
	for (size_t i = 0; i < items.size(); i++) {
		int32_t id = items[i]->getId();
		const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(id);
		if (itemTemplate != nullptr && itemTemplate->getRace() == player.getOppositeRace())
			continue;
		sb.append(utils::ChatUtil::item(id)).append(i + 1 < items.size() ? ", " : "");
	}
	utils::PacketSendUtility::sendMessage(player, sb);
}

} // namespace aion::gameserver::services::reward
