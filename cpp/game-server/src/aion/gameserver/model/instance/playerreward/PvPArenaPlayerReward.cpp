#include "aion/gameserver/model/instance/playerreward/PvPArenaPlayerReward.h"

#include <algorithm>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/instance/InstanceBuff.h"
#include "aion/gameserver/model/instance/instancescore/PvPArenaScore.h"
#include "aion/gameserver/model/templates/rewards/ArenaRewardItem.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::instance::playerreward {

// Java PvPArenaPlayerReward.java:31-33
PvPArenaPlayerReward::PvPArenaPlayerReward(gameobjects::player::Player& player, int32_t timeBonusValue, int8_t buffId)
	: PvPArenaPlayerReward(player.getObjectId(), player.getPlayerClass(), player.getName(), timeBonusValue, buffId) {
}

// Java PvPArenaPlayerReward.java:35-37
PvPArenaPlayerReward::PvPArenaPlayerReward(int32_t objectId, int32_t timeBonusValue, int8_t buffId)
	: PvPArenaPlayerReward(objectId, std::nullopt, std::string_view(), timeBonusValue, buffId) {
}

// Java PvPArenaPlayerReward.java:39-47 (the field initializers first: the four ArenaRewardItem(0, 0, 0, 0) defaults)
PvPArenaPlayerReward::PvPArenaPlayerReward(int32_t objectId, std::optional<PlayerClass> playerClassValue, std::string_view playerNameValue,
	int32_t timeBonusValue, int8_t buffId)
	: InstancePlayerReward(objectId), playerClass(playerClassValue), playerName(playerNameValue),
	  timeBonusModifier((static_cast<float>(timeBonusValue) / static_cast<float>(660000))),
	  ap(templates::rewards::ArenaRewardItem::create(0, 0, 0, 0)), gp(templates::rewards::ArenaRewardItem::create(0, 0, 0, 0)),
	  crucibleInsignia(templates::rewards::ArenaRewardItem::create(0, 0, 0, 0)),
	  courageInsignia(templates::rewards::ArenaRewardItem::create(0, 0, 0, 0)), boostMorale(InstanceBuff::create(buffId)) {
	setPoints(13000); // Java: super.setPoints(13000)
	timeBonus.set(timeBonusValue);
}

PvPArenaPlayerReward::~PvPArenaPlayerReward() = default;

runtime::Ref<PvPArenaPlayerReward> PvPArenaPlayerReward::create(gameobjects::player::Player& player, int32_t timeBonus, int8_t buffId) {
	return runtime::makeRef<PvPArenaPlayerReward>(player, timeBonus, buffId);
}

runtime::Ref<PvPArenaPlayerReward> PvPArenaPlayerReward::create(int32_t objectId, int32_t timeBonus, int8_t buffId) {
	return runtime::makeRef<PvPArenaPlayerReward>(objectId, timeBonus, buffId);
}

// Java PvPArenaPlayerReward.java:49-55
void PvPArenaPlayerReward::addPoints(int32_t pointsValue, instancescore::PvPArenaScore& instanceScore) {
	InstancePlayerReward::addPoints(pointsValue);
	if (getPoints() > instanceScore.getUpperScoreCap())
		setPoints(instanceScore.getUpperScoreCap());
	else if (getPoints() < instanceScore.getLowerScoreCap())
		setPoints(instanceScore.getLowerScoreCap());
}

// Java PvPArenaPlayerReward.java:57-59
PlayerClass PvPArenaPlayerReward::getPlayerClass() const {
	if (!playerClass)
		throw runtime::NullPointerException("PvPArenaPlayerReward.getPlayerClass()");
	return *playerClass;
}

// Java PvPArenaPlayerReward.java:73-75
int32_t PvPArenaPlayerReward::getTimeBonus() const {
	return std::max(timeBonus.get(), 0);
}

// Java PvPArenaPlayerReward.java:77-79
void PvPArenaPlayerReward::updateLogoutTime() {
	logoutTime.set(commons::utils::currentTimeMillis());
}

// Java PvPArenaPlayerReward.java:81-84
void PvPArenaPlayerReward::updateBonusTime() {
	int32_t offlineTime = static_cast<int32_t>(commons::utils::currentTimeMillis() - logoutTime.get());
	timeBonus.set(timeBonus.get() - static_cast<int32_t>(offlineTime * timeBonusModifier));
}

// Java PvPArenaPlayerReward.java:94-96
float PvPArenaPlayerReward::getParticipation() const {
	return static_cast<float>(getTimeBonus()) / static_cast<float>(timeBonus.get());
}

void PvPArenaPlayerReward::setAp(templates::rewards::ArenaRewardItem& value) {
	ap.set(runtime::Ref<templates::rewards::ArenaRewardItem>(value));
}

void PvPArenaPlayerReward::setGp(templates::rewards::ArenaRewardItem& value) {
	gp.set(runtime::Ref<templates::rewards::ArenaRewardItem>(value));
}

void PvPArenaPlayerReward::setCrucibleInsignia(templates::rewards::ArenaRewardItem& value) {
	crucibleInsignia.set(runtime::Ref<templates::rewards::ArenaRewardItem>(value));
}

void PvPArenaPlayerReward::setCourageInsignia(templates::rewards::ArenaRewardItem& value) {
	courageInsignia.set(runtime::Ref<templates::rewards::ArenaRewardItem>(value));
}

void PvPArenaPlayerReward::setRewardItem1(runtime::Ptr<templates::rewards::RewardItem> value) {
	rewardItem1.set(runtime::Ref<templates::rewards::RewardItem>(value));
}

void PvPArenaPlayerReward::setRewardItem2(runtime::Ptr<templates::rewards::RewardItem> value) {
	rewardItem2.set(runtime::Ref<templates::rewards::RewardItem>(value));
}

// Java PvPArenaPlayerReward.java:146-148
int32_t PvPArenaPlayerReward::getScorePoints() const {
	return timeBonus.get() + getPoints();
}

// Java PvPArenaPlayerReward.java:150-152
int32_t PvPArenaPlayerReward::getRemainingTime() const {
	return boostMorale->getRemainingTime();
}

// Java PvPArenaPlayerReward.java:154-156
bool PvPArenaPlayerReward::hasBoostMorale() const {
	return boostMorale->getRemainingTime() > 0;
}

// Java PvPArenaPlayerReward.java:158-160
void PvPArenaPlayerReward::applyBoostMoraleEffect(gameobjects::player::Player& player, int32_t duration) {
	boostMorale->applyEffect(player, duration);
}

// Java PvPArenaPlayerReward.java:162-164
void PvPArenaPlayerReward::endBoostMoraleEffect(gameobjects::player::Player& player) {
	boostMorale->endEffect(player);
}

} // namespace aion::gameserver::model::instance::playerreward
