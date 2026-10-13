#include "aion/gameserver/model/instance/playerreward/HarmonyGroupReward.h"

#include "aion/gameserver/model/autogroup/AGPlayer.h"
#include "aion/gameserver/model/templates/rewards/ArenaRewardItem.h"

namespace aion::gameserver::model::instance::playerreward {

// Java HarmonyGroupReward.java:17-21
HarmonyGroupReward::HarmonyGroupReward(int32_t objectId, int32_t timeBonus, int8_t buffId, int32_t grpObjectIdValue)
	: PvPArenaPlayerReward(objectId, timeBonus, buffId), grpObjectId(grpObjectIdValue) {
	setCourageInsignia(*templates::rewards::ArenaRewardItem::create(186000137, 0, 0, 0));
}

HarmonyGroupReward::~HarmonyGroupReward() = default;

runtime::Ref<HarmonyGroupReward> HarmonyGroupReward::create(int32_t objectId, int32_t timeBonus, int8_t buffId, int32_t grpObjectId) {
	return runtime::makeRef<HarmonyGroupReward>(objectId, timeBonus, buffId, grpObjectId);
}

// Java HarmonyGroupReward.java:23-25
std::vector<runtime::Ptr<autogroup::AGPlayer>> HarmonyGroupReward::getAssociatedPlayers() const {
	return players.snapshot();
}

// Java HarmonyGroupReward.java:27-29
bool HarmonyGroupReward::containsPlayer(int32_t playerObjectId) const {
	for (const runtime::Ptr<autogroup::AGPlayer>& agp : players.snapshot())
		if (agp->objectId() == playerObjectId)
			return true;
	return false;
}

// Java HarmonyGroupReward.java:31-38
runtime::Ptr<autogroup::AGPlayer> HarmonyGroupReward::getAGPlayer(int32_t playerObjectId) const {
	for (const runtime::Ptr<autogroup::AGPlayer>& agp : players.snapshot()) {
		if (agp->objectId() == playerObjectId) {
			return agp;
		}
	}
	return nullptr;
}

// Java HarmonyGroupReward.java:40-42
void HarmonyGroupReward::addPlayer(autogroup::AGPlayer& player) {
	players.add(runtime::Ref<autogroup::AGPlayer>(player));
}

} // namespace aion::gameserver::model::instance::playerreward
