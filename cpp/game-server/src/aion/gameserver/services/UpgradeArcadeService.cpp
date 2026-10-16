#include "aion/gameserver/services/UpgradeArcadeService.h"

#include <algorithm>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/UpgradeArcadeData.h"
#include "aion/gameserver/model/event/ArcadeProgress.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/event/upgradearcade/ArcadeLevel.h"
#include "aion/gameserver/model/templates/event/upgradearcade/ArcadeRewardItem.h"
#include "aion/gameserver/model/templates/event/upgradearcade/ArcadeRewards.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPGRADE_ARCADE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.UpgradeArcadeService@L100:45
//   com.aionemu.gameserver.services.UpgradeArcadeService@L105:45
//   com.aionemu.gameserver.services.UpgradeArcadeService@L124:45

namespace {

using model::event::ArcadeProgress;
using model::gameobjects::player::Player;
using model::templates::event::upgradearcade::ArcadeLevel;
using model::templates::event::upgradearcade::ArcadeRewardItem;
using model::templates::event::upgradearcade::ArcadeRewards;
using network::aion::serverpackets::SM_UPGRADE_ARCADE;
using utils::PacketSendUtility;

constexpr int32_t ARCADE_TOKEN = 186000389;

/** Java: DataManager.UPGRADE_ARCADE_DATA.getMaxUpgradeLevel() dereferenced - a null level is a NullPointerException */
const ArcadeLevel& maxUpgradeLevel() {
	const ArcadeLevel* level = dataholders::DataManager::UPGRADE_ARCADE_DATA->getMaxUpgradeLevel();
	if (level == nullptr)
		throw runtime::NullPointerException("UpgradeArcadeData.getMaxUpgradeLevel()");
	return *level;
}

} // namespace

// Java UpgradeArcadeService.java:36-39
runtime::Ptr<model::event::ArcadeProgress> UpgradeArcadeService::getProgress(int32_t objId) {
	runtime::Ptr<ArcadeProgress> progress = cachedProgress.putIfAbsent(objId, ArcadeProgress::create(objId));
	return progress != nullptr ? progress : cachedProgress.get(objId);
}

// Java UpgradeArcadeService.java:41-48
void UpgradeArcadeService::start(model::gameobjects::player::Player& player, int32_t sessionId) {
	ArcadeProgress& progress = *getProgress(player.getObjectId());
	PacketSendUtility::sendPacket(player, SM_UPGRADE_ARCADE(progress, sessionId));
	if (progress.getCurrentLevel() > 1)
		PacketSendUtility::sendPacket(player, SM_UPGRADE_ARCADE(progress));
	if (progress.getFrenzyEndTimeMillis() > commons::utils::currentTimeMillis())
		sendRemainingFrenzyModeTime(player, progress);
}

// Java UpgradeArcadeService.java:50-53
void UpgradeArcadeService::sendRemainingFrenzyModeTime(model::gameobjects::player::Player& player, model::event::ArcadeProgress& progress) {
	// Java: (int) ((end - now) / 1000), a long narrowed
	int32_t remainingFrenzyModeSeconds =
		static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>((progress.getFrenzyEndTimeMillis() - commons::utils::currentTimeMillis()) / 1000)));
	PacketSendUtility::sendPacket(player, SM_UPGRADE_ARCADE(std::max(0, remainingFrenzyModeSeconds)));
}

// Java UpgradeArcadeService.java:55-57
void UpgradeArcadeService::open(model::gameobjects::player::Player& player) {
	PacketSendUtility::sendPacket(player, SM_UPGRADE_ARCADE());
}

// Java UpgradeArcadeService.java:59-61
void UpgradeArcadeService::showRewardList(model::gameobjects::player::Player& player) {
	PacketSendUtility::sendPacket(player, SM_UPGRADE_ARCADE(UpgradeArcadeService::getInstance().getRewards()));
}

// Java UpgradeArcadeService.java:63-65: the data's list, as pointers into the static data
std::vector<const model::templates::event::upgradearcade::ArcadeRewards*> UpgradeArcadeService::getRewards() {
	std::vector<const ArcadeRewards*> rewards;
	for (const ArcadeRewards& r : dataholders::DataManager::UPGRADE_ARCADE_DATA->getRewards())
		rewards.push_back(&r);
	return rewards;
}

// Java UpgradeArcadeService.java:67-75
const model::templates::event::upgradearcade::ArcadeRewards* UpgradeArcadeService::getRewardsForLevel(int32_t level) {
	const std::vector<ArcadeRewards>& arcadeRewards = dataholders::DataManager::UPGRADE_ARCADE_DATA->getRewards();
	for (int32_t i = static_cast<int32_t>(arcadeRewards.size()) - 1; i >= 0; i--) {
		const ArcadeRewards& rewards = arcadeRewards[static_cast<size_t>(i)];
		if (level >= rewards.getMinLevel())
			return &rewards;
	}
	return nullptr;
}

// Java UpgradeArcadeService.java:77-112
void UpgradeArcadeService::startTry(model::gameobjects::player::Player& player) {
	ArcadeProgress& progress = *getProgress(player.getObjectId());
	int64_t nowMillis = commons::utils::currentTimeMillis();
	if (nowMillis < progress.getNextTryTimeMillis()) {
		utils::audit::AuditLogger::log(player, "tried to start next arcade try while the button was still greyed out");
		return;
	}
	if (progress.getCurrentLevel() >= maxUpgradeLevel().getLevel()) {
		return;
	} else if (progress.getCurrentLevel() == 0) {
		if (!player.getInventory().decreaseByItemId(ARCADE_TOKEN, 1))
			return;

		progress.setCurrentLevel(1);
		increaseFrenzyPoints(player, progress, FRENZY_POINTS_PER_TOKEN);
	} else if (progress.getCurrentLevel() == progress.getResumeLevel()) { // start after paying the tokens to resume
		// Java: FRENZY_POINTS_PER_TOKEN * EventsConfig.ARCADE_RESUME_TOKEN, int arithmetic
		increaseFrenzyPoints(player, progress,
			static_cast<int32_t>(static_cast<uint32_t>(FRENZY_POINTS_PER_TOKEN) * static_cast<uint32_t>(configs::main::EventsConfig::ARCADE_RESUME_TOKEN.load())));
	}
	int32_t delayMillis = 3000;
	progress.setTimeNextTry(nowMillis + delayMillis);
	bool success = commons::utils::Rnd::chance() < getUpgradeChance(progress.getCurrentLevel());
	PacketSendUtility::sendPacket(player, SM_UPGRADE_ARCADE(success, progress));
	if (success) {
		// Java lambda UpgradeArcadeService.java:100-103 (fieldmap UpgradeArcadeService@L100:45): pins the player and the progress
		utils::ThreadPoolManager::getInstance().schedule({&player, &progress}, [&player, &progress] {
			progress.setCurrentLevel(progress.getCurrentLevel() + 1);
			PacketSendUtility::sendPacket(player, SM_UPGRADE_ARCADE(progress));
		}, delayMillis);
	} else {
		// Java lambda UpgradeArcadeService.java:105-110 (fieldmap UpgradeArcadeService@L105:45): pins the player and the progress
		utils::ThreadPoolManager::getInstance().schedule({&player, &progress}, [&player, &progress] {
			bool canResume =
				progress.getResumeLevel() == 0 && progress.getCurrentLevel() >= dataholders::DataManager::UPGRADE_ARCADE_DATA->getMinResumableLevel();
			progress.setResumeLevel(canResume ? progress.getCurrentLevel() : 0);
			progress.setCurrentLevel(1);
			PacketSendUtility::sendPacket(player, SM_UPGRADE_ARCADE(progress, canResume));
		}, delayMillis);
	}
}

// Java UpgradeArcadeService.java:114-130
void UpgradeArcadeService::increaseFrenzyPoints(model::gameobjects::player::Player& player, model::event::ArcadeProgress& progress,
	int32_t frenzyPoints) {
	int32_t frenzyModeThreshold = 100;
	progress.setFrenzyPoints(static_cast<int32_t>(static_cast<uint32_t>(progress.getFrenzyPoints()) + static_cast<uint32_t>(frenzyPoints)));
	if (progress.getFrenzyPoints() >= frenzyModeThreshold) {
		progress.setFrenzyPoints(progress.getFrenzyPoints() % frenzyModeThreshold);
		int32_t frenzyDurationSeconds = 90;
		int64_t frenzyDurationMillis = frenzyDurationSeconds * 1000;
		progress.setFrenzyEndTimeMillis(commons::utils::currentTimeMillis() + frenzyDurationMillis);
		PacketSendUtility::sendPacket(player, SM_UPGRADE_ARCADE(frenzyDurationSeconds));
		int32_t playerId = player.getObjectId();
		// Java lambda UpgradeArcadeService.java:124-128 (fieldmap UpgradeArcadeService@L124:45): pins this (the immortal service) and the
		// progress; the player is looked up again by id
		utils::ThreadPoolManager::getInstance().schedule({this, &progress}, [this, &progress, playerId] {
			runtime::Ptr<Player> p = world::World::getInstance().getPlayer(playerId);
			if (p != nullptr)
				sendRemainingFrenzyModeTime(*p, progress);
		}, frenzyDurationMillis);
	}
}

// Java UpgradeArcadeService.java:132-136
float UpgradeArcadeService::getUpgradeChance(int32_t currentLevel) {
	const ArcadeLevel* lv = nullptr;
	for (const ArcadeLevel& level : dataholders::DataManager::UPGRADE_ARCADE_DATA->getUpgradeLevels()) {
		if (level.getLevel() == currentLevel) {
			lv = &level;
			break;
		}
	}
	return lv == nullptr ? maxUpgradeLevel().getUpgradeChance() : lv->getUpgradeChance();
}

// Java UpgradeArcadeService.java:138-150
void UpgradeArcadeService::resume(model::gameobjects::player::Player& player) {
	ArcadeProgress& progress = *getProgress(player.getObjectId());
	if (progress.getResumeLevel() == 0) {
		utils::audit::AuditLogger::log(player, "illegally tried to resume arcade");
		return;
	}
	if (!player.getInventory().decreaseByItemId(ARCADE_TOKEN, configs::main::EventsConfig::ARCADE_RESUME_TOKEN.load())) {
		PacketSendUtility::sendPacket(player, SM_UPGRADE_ARCADE(8, true));
		return;
	}
	progress.setCurrentLevel(progress.getResumeLevel());
	PacketSendUtility::sendPacket(player, SM_UPGRADE_ARCADE(progress));
}

// Java UpgradeArcadeService.java:152-181
void UpgradeArcadeService::getReward(model::gameobjects::player::Player& player) {
	ArcadeProgress& progress = *getProgress(player.getObjectId());
	if (progress.getCurrentLevel() == 0) {
		utils::audit::AuditLogger::log(player, "tried to get arcade rewards without spending token");
		return;
	}
	std::vector<const ArcadeRewardItem*> rewardList;

	const ArcadeRewards* rewards = getRewardsForLevel(progress.getCurrentLevel());
	if (rewards == nullptr)
		return;
	bool isFrenzyActive = commons::utils::currentTimeMillis() < progress.getFrenzyEndTimeMillis();
	for (const ArcadeRewardItem& arcadeTabItem : rewards->getArcadeRewardItems()) {
		if (isFrenzyActive) {
			if (arcadeTabItem.getFrenzyCount() > 0)
				rewardList.push_back(&arcadeTabItem);
		} else if (arcadeTabItem.getNormalCount() > 0) {
			rewardList.push_back(&arcadeTabItem);
		}
	}

	const ArcadeRewardItem* const* chosen = commons::utils::Rnd::get(rewardList); // Java Rnd.get(List): null for an empty list
	const ArcadeRewardItem* item = chosen != nullptr ? *chosen : nullptr;
	if (item != nullptr) {
		int64_t itemCount = isFrenzyActive ? item->getFrenzyCount() : item->getNormalCount();
		item::ItemService::addItem(player, item->getItemId(), itemCount, true);
		PacketSendUtility::sendPacket(player, SM_UPGRADE_ARCADE(item->getItemId(), itemCount));
		progress.setResumeLevel(0);
		progress.setCurrentLevel(0);
	}
}

UpgradeArcadeService& UpgradeArcadeService::getInstance() {
	static UpgradeArcadeService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services
