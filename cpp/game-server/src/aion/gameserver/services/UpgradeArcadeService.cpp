#include "aion/gameserver/services/UpgradeArcadeService.h"

#include "aion/gameserver/model/event/ArcadeProgress.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.UpgradeArcadeService@L100:45
//   com.aionemu.gameserver.services.UpgradeArcadeService@L105:45
//   com.aionemu.gameserver.services.UpgradeArcadeService@L124:45

runtime::Ptr<model::event::ArcadeProgress> UpgradeArcadeService::getProgress(int32_t objId) {
	AION_UNPORTED();
}

void UpgradeArcadeService::start(model::gameobjects::player::Player& player, int32_t sessionId) {
	AION_UNPORTED();
}

void UpgradeArcadeService::sendRemainingFrenzyModeTime(model::gameobjects::player::Player& player, model::event::ArcadeProgress& progress) {
	AION_UNPORTED();
}

void UpgradeArcadeService::open(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void UpgradeArcadeService::showRewardList(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

std::vector<const model::templates::event::upgradearcade::ArcadeRewards*> UpgradeArcadeService::getRewards() {
	AION_UNPORTED();
}

const model::templates::event::upgradearcade::ArcadeRewards* UpgradeArcadeService::getRewardsForLevel(int32_t level) {
	AION_UNPORTED();
}

void UpgradeArcadeService::startTry(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void UpgradeArcadeService::increaseFrenzyPoints(model::gameobjects::player::Player& player, model::event::ArcadeProgress& progress,
	int32_t frenzyPoints) {
	AION_UNPORTED();
}

float UpgradeArcadeService::getUpgradeChance(int32_t currentLevel) {
	AION_UNPORTED();
}

void UpgradeArcadeService::resume(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void UpgradeArcadeService::getReward(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

UpgradeArcadeService& UpgradeArcadeService::getInstance() {
	static UpgradeArcadeService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services
