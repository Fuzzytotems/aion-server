#include "aion/gameserver/services/FactionPackService.h"

#include "aion/gameserver/dao/FactionPackDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/mail/SystemMailService.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::services {

// Java LocalDateTime.of(year, month, day, hour, minute, second)
static std::chrono::local_time<std::chrono::milliseconds> localDateTime(int year, unsigned month, unsigned day, int hour, int minute, int second) {
	return std::chrono::local_days{std::chrono::year{year} / std::chrono::month{month} / std::chrono::day{day}} + std::chrono::hours{hour} +
		std::chrono::minutes{minute} + std::chrono::seconds{second};
}

FactionPackService::FactionPackService()
	: elyosMinCreationTime(localDateTime(2020, 9, 14, 0, 0, 0)), elyosMaxCreationTime(localDateTime(2020, 9, 26, 23, 59, 59)),
	  asmodianMinCreationTime(localDateTime(2022, 6, 18, 0, 0, 0)), asmodianMaxCreationTime(localDateTime(2022, 7, 19, 23, 59, 59)) {
	rewards.add(model::templates::rewards::RewardItem::create(186000236, 500)); // Blood Mark
	rewards.add(model::templates::rewards::RewardItem::create(162002030, 250)); // [Event] Premium Restoration Serum
	rewards.add(model::templates::rewards::RewardItem::create(162000023, 100)); // Greater Healing Potion
	rewards.add(model::templates::rewards::RewardItem::create(166000195, 50));  // Epsilon Enchantment Stone
	rewards.add(model::templates::rewards::RewardItem::create(169630007, 1));   // [Expand Card] Expand Cube Ticket (lvl 4)
	rewards.add(model::templates::rewards::RewardItem::create(188053526, 5));   // [Event] Aion's Steel Form Candy Box
}

FactionPackService::~FactionPackService() = default;

FactionPackService& FactionPackService::getInstance() {
	static FactionPackService instance; // Java SingletonHolder
	return instance;
}

void FactionPackService::addPlayerCustomReward(model::gameobjects::player::Player& player) {
	if (rewards.isEmpty() || player.getLevel() != 65 || (player.getCommonData()->getMailboxLetters() + rewards.size() > 100))
		return;
	if (player.getRace() == model::Race::ASMODIANS)
		sendRewards(player, asmodianMinCreationTime, asmodianMaxCreationTime);
	else
		sendRewards(player, elyosMinCreationTime, elyosMaxCreationTime);
}

void FactionPackService::sendRewards(model::gameobjects::player::Player& player, std::chrono::local_time<std::chrono::milliseconds> minCreationTime, std::chrono::local_time<std::chrono::milliseconds> maxCreationTime) {
	// Java: the null checks of the LocalDateTime parameters never match (the fields are final and set)
	std::chrono::local_time<std::chrono::milliseconds> creationTime =
		utils::time::ServerTime::ofEpochMilli(player.getAccount()->getCreationDate()).get_local_time();
	if (creationTime < minCreationTime)
		return;
	if (creationTime > maxCreationTime)
		return;
	int32_t accountId = player.getAccount()->getId();
	if (dao::FactionPackDAO::loadReceivingPlayer(accountId) > 0)
		return;
	if (!dao::FactionPackDAO::storeReceivingPlayer(accountId, player.getObjectId()))
		return;
	for (const runtime::Ptr<model::templates::rewards::RewardItem>& e : rewards.snapshot()) {
		const model::templates::item::ItemTemplate* template_ = dataholders::DataManager::ITEM_DATA->getItemTemplate(e->getId());
		if (template_ != nullptr && template_->getRace() == player.getOppositeRace())
			continue;
		mail::SystemMailService::sendMail("Beyond Aion", player.getName(), "Faction Pack",
			"Greetings Daeva!\n\n"
			"In gratitude for your decision to join this faction we prepared an additional item pack.\n\n"
			"Enjoy your stay on Beyond Aion!",
			e->getId(), e->getCount(), 0, model::gameobjects::LetterType::EXPRESS);
	}
}

} // namespace aion::gameserver::services
