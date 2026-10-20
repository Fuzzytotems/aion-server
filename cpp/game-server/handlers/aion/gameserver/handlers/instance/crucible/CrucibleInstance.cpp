#include "aion/gameserver/handlers/instance/crucible/CrucibleInstance.h"

#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/CruciblePlayerReward.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance::crucible {

// Java CrucibleInstance.java:25-27
CrucibleInstance::CrucibleInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<CrucibleInstance> CrucibleInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<CrucibleInstance>(instance);
}

// Java CrucibleInstance.java:29-34
void CrucibleInstance::onEnterInstance(Player& player) {
	if (!instanceScore.get()->containsPlayer(player.getObjectId())) {
		addPlayerReward(player);
	}
}

// Java CrucibleInstance.java:36-39
void CrucibleInstance::onInstanceCreate() {
	instanceScore = InstanceScore::create();
}

// Java CrucibleInstance.java:41-43
void CrucibleInstance::addPlayerReward(Player& player) {
	instanceScore.get()->addPlayerReward(*CruciblePlayerReward::create(player.getObjectId()));
}

// Java CrucibleInstance.java:45-47
runtime::Ptr<CruciblePlayerReward> CrucibleInstance::getPlayerReward(int32_t objectId) {
	return runtime::cast<CruciblePlayerReward>(instanceScore.get()->getPlayerReward(objectId));
}

// Java CrucibleInstance.java:49-52
runtime::Ptr<InstanceScore> CrucibleInstance::getInstanceScore() {
	return instanceScore.get();
}

// Java CrucibleInstance.java:54-59
std::optional<std::vector<runtime::Ptr<Npc>>> CrucibleInstance::getNpcs(int32_t npcId) {
	if (!isInstanceDestroyed) {
		return instance->getNpcs({npcId});
	}
	return std::nullopt;
}

// Java CrucibleInstance.java:61-63
bool CrucibleInstance::isInZone(const ZoneName* zone, Player& player) {
	return player.isInsideZone(zone);
}

// Java CrucibleInstance.java:65-67
void CrucibleInstance::teleport(Player& player, float x, float y, float z, int8_t h) {
	TeleportService::teleportTo(player, *instance, x, y, z, h);
}

// Java CrucibleInstance.java:69-72
StageType CrucibleInstance::getStage() {
	return stageType;
}

// Java CrucibleInstance.java:74-79
bool CrucibleInstance::onReviveEvent(Player& player) {
	PlayerReviveService::revive(player, 100, 100, false, 0);
	player.getGameStats()->updateStatsAndSpeedVisually();
	return true;
}

// Java CrucibleInstance.java:81-85
void CrucibleInstance::onInstanceDestroy() {
	isInstanceDestroyed = true;
	instanceScore.get()->clear();
}

// Java CrucibleInstance.java:87-90
bool CrucibleInstance::allowSelfReviveBySkill() {
	return false;
}

// Java CrucibleInstance.java:92-95
bool CrucibleInstance::allowSelfReviveByItem() {
	return false;
}

} // namespace aion::gameserver::handlers::instance::crucible
