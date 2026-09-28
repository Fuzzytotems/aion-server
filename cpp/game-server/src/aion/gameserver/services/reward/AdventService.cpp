#include "aion/gameserver/services/reward/AdventService.h"

#include <chrono>

#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/dao/AdventDAO.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"
#include "aion/gameserver/runtime/base/Unported.h"
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

void AdventService::redeemReward(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void AdventService::showTodaysReward(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::reward
