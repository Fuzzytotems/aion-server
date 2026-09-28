#include "aion/gameserver/custom/pvpmap/PvpMapHandler.h"

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::custom::pvpmap {

// Stored lambdas and anonymous classes of the Java class (hub-headers.md §7.3), defined here when their bodies are ported:
//   anonymous ItemUseObserver at PvpMapHandler.java:232 (fieldmap PvpMapHandler_ItemUseObserver)
//   method_ref at line 105, lambdas at lines 118, 145, 157, 179, 189, 211, 213

PvpMapHandler::PvpMapHandler(world::WorldMapInstance& instanceValue) : GeneralInstanceHandler(instanceValue) {
}

PvpMapHandler::~PvpMapHandler() = default;

runtime::Ref<PvpMapHandler> PvpMapHandler::create(world::WorldMapInstance& instanceValue) {
	return runtime::makeRef<PvpMapHandler>(instanceValue);
}

void PvpMapHandler::onInstanceCreate() {
	AION_UNPORTED();
}

void PvpMapHandler::spawnShugo(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PvpMapHandler::startSupplyTask() {
	AION_UNPORTED();
}

void PvpMapHandler::scheduleSupplySpawn() {
	AION_UNPORTED();
}

void PvpMapHandler::spawnKeymasters() {
	AION_UNPORTED();
}

void PvpMapHandler::spawnKeymasterOrTreasureChest(int32_t npcId, bool isKeymaster) {
	AION_UNPORTED();
}

void PvpMapHandler::scheduleRespawn(int32_t npcId, int32_t time, bool isKeymaster) {
	AION_UNPORTED();
}

void PvpMapHandler::spawnTreasureChests() {
	AION_UNPORTED();
}

void PvpMapHandler::startRandomBossTask() {
	AION_UNPORTED();
}

void PvpMapHandler::scheduleRandomBossDespawn() {
	AION_UNPORTED();
}

void PvpMapHandler::scheduleSupplyDespawn() {
	AION_UNPORTED();
}

void PvpMapHandler::join(model::gameobjects::player::Player& p) {
	AION_UNPORTED();
}

void PvpMapHandler::leave(model::gameobjects::player::Player& p) {
	AION_UNPORTED();
}

void PvpMapHandler::startTeleportation(model::gameobjects::player::Player& p, bool isLeaving) {
	AION_UNPORTED();
}

runtime::Ref<controllers::observer::ActionObserver> PvpMapHandler::getAllObserver(model::gameobjects::player::Player& p) {
	AION_UNPORTED();
}

bool PvpMapHandler::canJoin(model::gameobjects::player::Player& p) {
	AION_UNPORTED();
}

bool PvpMapHandler::checkState(model::gameobjects::player::Player& p) {
	AION_UNPORTED();
}

void PvpMapHandler::updateOrigin(model::gameobjects::player::Player& p) {
	AION_UNPORTED();
}

void PvpMapHandler::updateJoinOrLeaveTime(model::gameobjects::player::Player& p) {
	AION_UNPORTED();
}

bool PvpMapHandler::onReviveEvent(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool PvpMapHandler::onDie(model::gameobjects::player::Player& player, model::gameobjects::Creature& lastAttacker) {
	AION_UNPORTED();
}

void PvpMapHandler::onDie(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void PvpMapHandler::handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player> player, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void PvpMapHandler::announceDeath(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PvpMapHandler::onEnterInstance(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PvpMapHandler::onLeaveInstance(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PvpMapHandler::onPlayerLogout(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PvpMapHandler::onInstanceDestroy() {
	AION_UNPORTED();
}

void PvpMapHandler::cancelTasks() {
	AION_UNPORTED();
}

bool PvpMapHandler::spawnAllowed() {
	AION_UNPORTED();
}

int32_t PvpMapHandler::getParticipantsSize() {
	int32_t playerCount = 0;
	for (const runtime::Ptr<model::gameobjects::player::Player>& p : instance->getPlayersInside()) {
		if (!p->isStaff()) {
			playerCount++;
		}
	}
	return playerCount;
}

void PvpMapHandler::removePlayer(model::gameobjects::player::Player& p) {
	AION_UNPORTED();
}

void PvpMapHandler::revive(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool PvpMapHandler::isAtVulnerableFortress(int32_t worldId, float x, float y, float z) {
	runtime::Ptr<model::siege::FortressLocation> fortress = services::SiegeService::getInstance().findFortress(worldId, x, y, z);
	return fortress && fortress->isVulnerable();
}

bool PvpMapHandler::isOnMap(model::gameobjects::Creature& creature) {
	return instance && instance->getObject(creature.getObjectId());
}

bool PvpMapHandler::isRandomBoss(int32_t objectId) {
	return currentRandomBossObjId.get() == objectId;
}

bool PvpMapHandler::isRandomBossAlive() {
	// Java: (Npc) instance.getObject(currentRandomBossObjId) (ClassCastException for another object type)
	runtime::Ptr<model::gameobjects::Npc> boss = runtime::cast<model::gameobjects::Npc>(instance->getObject(currentRandomBossObjId.get()));
	return boss && !boss->isDead();
}

void PvpMapHandler::spawnNpcs() {
	AION_UNPORTED();
}

void PvpMapHandler::addRespawnLocations() {
	AION_UNPORTED();
}

void PvpMapHandler::addSupplyPositions() {
	AION_UNPORTED();
}

void PvpMapHandler::addKeymasterPositions() {
	AION_UNPORTED();
}

void PvpMapHandler::addTreasurePositions() {
	AION_UNPORTED();
}

std::string PvpMapHandler::getZoneNameL10n(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

int32_t PvpMapHandler::getZoneNameL10nId(std::string_view zoneName) {
	AION_UNPORTED();
}

float PvpMapHandler::getApMultiplier() {
	return configs::main::CustomConfig::PVP_MAP_PVE_AP_MULTIPLIER.load();
}

} // namespace aion::gameserver::custom::pvpmap
