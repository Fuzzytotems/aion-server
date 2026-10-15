#include "aion/gameserver/model/instance/instancescore/HarmonyArenaScore.h"

#include <algorithm>

#include "aion/gameserver/model/autogroup/AGPlayer.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/instance/playerreward/HarmonyGroupReward.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::model::instance::instancescore {

// Java HarmonyArenaScore.java:20-22
HarmonyArenaScore::HarmonyArenaScore(world::WorldMapInstance& instanceValue) : PvPArenaScore(instanceValue) {
}

HarmonyArenaScore::~HarmonyArenaScore() = default;

runtime::Ref<HarmonyArenaScore> HarmonyArenaScore::create(world::WorldMapInstance& instance) {
	return runtime::makeRef<HarmonyArenaScore>(instance);
}

// Java HarmonyArenaScore.java:24-26
runtime::Ptr<playerreward::HarmonyGroupReward> HarmonyArenaScore::getGroupReward(int32_t playerId) {
	for (const runtime::Ptr<playerreward::HarmonyGroupReward>& reward : groups.snapshot())
		if (reward->containsPlayer(playerId))
			return reward;
	return nullptr;
}

// Java HarmonyArenaScore.java:28-40
std::vector<runtime::Ptr<playerreward::HarmonyGroupReward>> HarmonyArenaScore::getHarmonyGroupInside() {
	std::vector<runtime::Ptr<playerreward::HarmonyGroupReward>> harmonyGroups;
	for (const runtime::Ptr<playerreward::HarmonyGroupReward>& group : groups.snapshot()) {
		for (const runtime::Ptr<autogroup::AGPlayer>& agp : group->getAssociatedPlayers()) {
			runtime::Ptr<gameobjects::player::Player> p = instance.get()->getPlayer(agp->objectId());
			if (p != nullptr) {
				harmonyGroups.push_back(group);
				break;
			}
		}
	}
	return harmonyGroups;
}

// Java HarmonyArenaScore.java:42-50
std::vector<runtime::Ptr<gameobjects::player::Player>> HarmonyArenaScore::getPlayersInside(playerreward::HarmonyGroupReward& group) {
	std::vector<runtime::Ptr<gameobjects::player::Player>> players;
	for (const runtime::Ptr<gameobjects::player::Player>& playerInside : instance.get()->getPlayersInside()) {
		if (group.containsPlayer(playerInside->getObjectId())) {
			players.push_back(playerInside);
		}
	}
	return players;
}

// Java HarmonyArenaScore.java:52-54
void HarmonyArenaScore::addHarmonyGroup(playerreward::HarmonyGroupReward& reward) {
	groups.add(runtime::Ref<playerreward::HarmonyGroupReward>(reward));
}

// Java HarmonyArenaScore.java:56-58
std::vector<runtime::Ptr<playerreward::HarmonyGroupReward>> HarmonyArenaScore::getGroups() {
	return groups.snapshot();
}

// Java HarmonyArenaScore.java:60-70
int32_t HarmonyArenaScore::getRank(playerreward::PvPArenaPlayerReward& reward) {
	std::vector<runtime::Ptr<playerreward::HarmonyGroupReward>> sortedByPoints = groups.snapshot();
	std::stable_sort(sortedByPoints.begin(), sortedByPoints.end(),
		[](const auto& a, const auto& b) { return a->getScorePoints() < b->getScorePoints(); });

	int32_t rank = -1;
	for (const runtime::Ptr<playerreward::HarmonyGroupReward>& r : sortedByPoints) {
		if (r->getScorePoints() >= reward.getScorePoints())
			rank++;
	}
	return rank;
}

// Java HarmonyArenaScore.java:72-75
int32_t HarmonyArenaScore::getTotalPoints() {
	int32_t sum = 0;
	for (const runtime::Ptr<playerreward::HarmonyGroupReward>& group : groups.snapshot())
		sum = static_cast<int32_t>(static_cast<uint32_t>(sum) + static_cast<uint32_t>(group->getScorePoints()));
	return sum;
}

// Java HarmonyArenaScore.java:77-81
void HarmonyArenaScore::clear() {
	groups.clear();
	PvPArenaScore::clear();
}

} // namespace aion::gameserver::model::instance::instancescore
