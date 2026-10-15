#include "aion/gameserver/services/reward/WebRewardService.h"

#include <exception>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dao/RewardServiceDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/ItemId.h"
#include "aion/gameserver/model/templates/rewards/RewardEntryItem.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/services/mail/SystemMailService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::services::reward {

static const auto log = commons::logging::LoggerFactory::getLogger("WEB_REWARDS_LOG");

namespace {

using item::ItemService;
using model::PlayerClass;
using model::Race;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_QUEST_ACTION;
using questEngine::model::QuestStatus;
using utils::PacketSendUtility;

} // namespace

// Java WebRewardService.java:102-104
bool WebRewardService::MaxLevelReward::isPendingAscension(model::gameobjects::player::Player& player) {
	return pendingAscension.contains(player.getObjectId());
}

// Java WebRewardService.java:106-140
bool WebRewardService::MaxLevelReward::reward(model::gameobjects::player::Player& player) {
	if (!player.getCommonData()->isDaeva()) {
		if (!pendingAscension.add(player.getObjectId()))
			return false;
		if (player.getLevel() < 9)
			player.getCommonData()->setLevel(9); // reaching level 9 starts the ascension quest
		const int32_t questNpcId = player.getRace() == Race::ELYOS ? 790001 : 203550; // Pernos / Munin
		int32_t questId = player.getRace() == Race::ELYOS ? 1006 : 2008;
		int32_t questVar = player.getRace() == Race::ELYOS ? 5 : 6;
		runtime::Ptr<questEngine::model::QuestState> qs = player.getQuestStateList()->getQuestState(questId);
		if (qs == nullptr) // Java: qs.getStatus() on null
			throw runtime::NullPointerException("QuestStateList.getQuestState(" + std::to_string(questId) + ")");
		if (qs->getStatus() != QuestStatus::REWARD) { // class selection is complete at this point
			if (qs->getStatus() == QuestStatus::COMPLETE) { // if player switched back to a starting class (GM command or DB change)
				qs->setStatus(QuestStatus::START);
				PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::ADD, *qs));
			}
			if (qs->getQuestVars()->getQuestVars() != questVar)
				qs->setQuestVar(questVar);
			PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::UPDATE, *qs));
		}
		runtime::Ptr<model::gameobjects::VisibleObject> questNpc = player.getKnownList().findObject([questNpcId](world::knownlist::KnownObject& o) {
			runtime::Ptr<model::gameobjects::Npc> npc = runtime::as<model::gameobjects::Npc>(o.get());
			return npc != nullptr && npc->getNpcId() == questNpcId;
		});
		if (questNpc == nullptr || utils::PositionUtil::getDistance(player, *questNpc) >= 20)
			teleport::TeleportService::sendTeleportRequest(player, questNpcId); // completing the quest (updates daeva status) calls the reward method again
	} else {
		int32_t maxLevel = dataholders::DataManager::PLAYER_EXPERIENCE_TABLE->getMaxLevel() - 1; // max is 66
		if (player.getLevel() >= maxLevel)
			return false;
		pendingAscension.remove(player.getObjectId());
		addBasicGear(player);
		player.getCommonData()->setLevel(maxLevel);
		std::string message = utils::ChatUtil::l10n(904804) + " Level " + std::to_string(player.getLevel()); // You receive the following reward: Level 65
		PacketSendUtility::sendMessage(player, message, model::ChatType::BRIGHT_YELLOW);
		teleport::TeleportService::sendTeleportRequest(player, player.getRace() == Race::ELYOS ? 798926 : 799225); // Outremus / Richelle for daevanion quests
	}
	return true;
}

// Java WebRewardService.java:142-218
void WebRewardService::MaxLevelReward::addBasicGear(model::gameobjects::player::Player& player) {
	ItemService::addItem(player, 188053624, 10, true); // Unified Return Scroll Bundle
	switch (player.getPlayerClass()) { // weapons
		case PlayerClass::GLADIATOR:
			ItemService::addItem(player, 101300728, 1, true); // Transient Lance (14 days)
			break;
		case PlayerClass::TEMPLAR:
			ItemService::addItem(player, 100900749, 1, true); // Transient Greatsword (14 days)
			break;
		case PlayerClass::ASSASSIN:
			ItemService::addItem(player, 100000993, 1, true); // Transient Brand (14 days)
			ItemService::addItem(player, 100200882, 1, true); // Transient Dirk (14 days)
			break;
		case PlayerClass::RANGER:
			ItemService::addItem(player, 101700795, 1, true); // Transient Bow (14 days)
			break;
		case PlayerClass::SORCERER:
			ItemService::addItem(player, 100600830, 1, true); // Transient Tome (14 days)
			break;
		case PlayerClass::SPIRIT_MASTER:
			ItemService::addItem(player, 100500775, 1, true); // Transient Sphere (14 days)
			break;
		case PlayerClass::CLERIC:
			ItemService::addItem(player, 100100755, 1, true); // Transient Warhammer (14 days)
			ItemService::addItem(player, 115001049, 1, true); // Transient Shield (14 days)
			break;
		case PlayerClass::CHANTER:
			ItemService::addItem(player, 101500778, 1, true); // Transient Staff (14 days)
			break;
		case PlayerClass::RIDER:
			ItemService::addItem(player, 102101083, 1, true); // Atreian Faithful Cipher-Blade
			break;
		case PlayerClass::GUNNER:
			ItemService::addItem(player, 101800899, 2, true); // Transient Pistol (14 days)
			break;
		case PlayerClass::BARD:
			ItemService::addItem(player, 102000923, 1, true); // Transient Harp (14 days)
			break;
		default:
			break;
	}
	switch (player.getPlayerClass()) { // armor
		case PlayerClass::GLADIATOR:
		case PlayerClass::TEMPLAR:
			ItemService::addItem(player, 110601053, 1, true); // Transient Breastplate (14 days)
			ItemService::addItem(player, 111601030, 1, true); // Transient Gauntlets (14 days)
			ItemService::addItem(player, 112601003, 1, true); // Transient Shoulderplates (14 days)
			ItemService::addItem(player, 113601014, 1, true); // Transient Greaves (14 days)
			ItemService::addItem(player, 114601010, 1, true); // Transient Sabatons (14 days)
			break;
		case PlayerClass::ASSASSIN:
		case PlayerClass::RANGER:
		case PlayerClass::GUNNER:
			ItemService::addItem(player, 110301102, 1, true); // Transient Jerkin (14 days)
			ItemService::addItem(player, 111301057, 1, true); // Transient Vambrace (14 days)
			ItemService::addItem(player, 112301002, 1, true); // Transient Shoulderguards (14 days)
			ItemService::addItem(player, 113301074, 1, true); // Transient Breeches (14 days)
			ItemService::addItem(player, 114301109, 1, true); // Transient Boots (14 days)
			break;
		case PlayerClass::SORCERER:
		case PlayerClass::SPIRIT_MASTER:
		case PlayerClass::BARD:
			ItemService::addItem(player, 110101165, 1, true); // Transient Tunic (14 days)
			ItemService::addItem(player, 111101056, 1, true); // Transient Gloves (14 days)
			ItemService::addItem(player, 112101014, 1, true); // Transient Pauldrons (14 days)
			ItemService::addItem(player, 113101069, 1, true); // Transient Leggings (14 days)
			ItemService::addItem(player, 114101097, 1, true); // Transient Shoes (14 days)
			break;
		case PlayerClass::CLERIC:
		case PlayerClass::CHANTER:
		case PlayerClass::RIDER:
			ItemService::addItem(player, 110501071, 1, true); // Transient Hauberk (14 days)
			ItemService::addItem(player, 111501041, 1, true); // Transient Handguards (14 days)
			ItemService::addItem(player, 112500990, 1, true); // Transient Spaulders (14 days)
			ItemService::addItem(player, 113501049, 1, true); // Transient Chausses (14 days)
			ItemService::addItem(player, 114501057, 1, true); // Transient Brogans (14 days)
			break;
		default:
			break;
	}
}

WebRewardService& WebRewardService::getInstance() {
	static WebRewardService instance; // Java SingletonHolder
	return instance;
}

WebRewardService::WebRewardService() = default;

// Java WebRewardService.java:47-70
void WebRewardService::sendAvailableRewards(runtime::Ptr<model::gameobjects::player::Player> player) {
	if (player == nullptr)
		return;
	std::vector<runtime::Ref<model::templates::rewards::RewardEntryItem>> list = dao::RewardServiceDAO::loadUnreceived(player->getObjectId());
	if (list.size() == 0)
		return;

	std::vector<int32_t> rewarded;
	for (const runtime::Ref<model::templates::rewards::RewardEntryItem>& item : list) {
		try {
			if (sendRewardItem(*player, *item) || executeRewardAction(*player, *item)) {
				log.info("[WebRewardService][" + std::to_string(item->getEntryId()) + "] " + player->toString() + " has received " + item->toString());
				rewarded.push_back(item->getEntryId());
			} else {
				log.warn("[WebRewardService][" + std::to_string(item->getEntryId()) + "] " + player->toString() + " could not receive " + item->toString());
			}
		} catch (const std::exception& e) { // Java: catch (Exception e)
			log.error("[WebRewardService][" + std::to_string(item->getEntryId()) + "] error adding " + item->toString() + " to " + player->toString(), e);
		}
	}

	if (rewarded.size() > 0)
		dao::RewardServiceDAO::storeReceived(rewarded, commons::utils::currentTimeMillis());
}

// Java WebRewardService.java:72-87
bool WebRewardService::sendRewardItem(model::gameobjects::player::Player& player, model::templates::rewards::RewardEntryItem& item) {
	if (dataholders::DataManager::ITEM_DATA->getItemTemplate(item.getId()) == nullptr)
		return false;

	int32_t itemId = 0;
	int64_t kinahCount = 0, itemCount = 0;
	if (item.getId() == model::items::ItemId::KINAH) {
		kinahCount = item.getCount();
	} else {
		itemId = item.getId();
		itemCount = item.getCount();
	}

	return mail::SystemMailService::sendMail("$$CASH_ITEM_MAIL", player.getName(), std::to_string(item.getId()) + ", " + std::to_string(item.getCount()),
		"0, " + std::to_string(commons::utils::currentTimeMillis() / 1000) + ",", itemId, itemCount, kinahCount, model::gameobjects::LetterType::BLACKCLOUD);
}

// Java WebRewardService.java:89-96
bool WebRewardService::executeRewardAction(model::gameobjects::player::Player& player, model::templates::rewards::RewardEntryItem& rewardItem) {
	switch (rewardItem.getId()) {
		case 1:
			return MaxLevelReward::reward(player);
		default:
			return false;
	}
}

} // namespace aion::gameserver::services::reward
