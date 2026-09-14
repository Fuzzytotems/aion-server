#include "aion/gameserver/controllers/RVController.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/observer/TerrainZoneCollisionMaterialActor.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

namespace aion::gameserver::controllers {

RVController::RVController(runtime::Ptr<model::gameobjects::Npc> value, services::rift::RiftEnum riftTemplateValue)
	: isMaster_(true), isVortex_(), isVolatile_(), isInvasion_(), slaveSpawnTemplate(), slave(), maxEntries(), minLevel(), maxLevel(),
	  isAccepting(true), riftTemplate(riftTemplateValue), deSpawnedTime() {
	// Java: this.isVortex = riftTemplate.isVortex(); this.maxEntries = riftTemplate.getEntries(); this.minLevel = riftTemplate.getMinLevel();
	// this.maxLevel = riftTemplate.getMaxLevel(); this.deSpawnedTime = ((int) (System.currentTimeMillis() / 1000)) + (isVortex ?
	// VortexService.getInstance().getDuration() * 3600 : RiftService.getInstance().getDuration() * 3600); this.isInvasion =
	// riftTemplate.isInvasionRift(); if (slave != null) { this.slave = slave; this.slaveSpawnTemplate = slave.getSpawn(); }
	AION_UNPORTED();
}

RVController::RVController(runtime::Ptr<model::gameobjects::Npc> value, services::rift::RiftEnum riftTemplateValue, bool isWithGuards)
	: isMaster_(true), isVortex_(), isVolatile_(), isInvasion_(), slaveSpawnTemplate(), slave(), maxEntries(), minLevel(), maxLevel(),
	  isAccepting(true), riftTemplate(riftTemplateValue), deSpawnedTime() {
	// Java: this.isVortex = riftTemplate.isVortex(); this.maxEntries = riftTemplate.getEntries(); this.minLevel = riftTemplate.getMinLevel();
	// this.maxLevel = riftTemplate.getMaxLevel(); this.deSpawnedTime = ((int) (System.currentTimeMillis() / 1000)) + (isVortex ?
	// VortexService.getInstance().getDuration() * 3600 : RiftService.getInstance().getDuration() * 3600); this.isInvasion =
	// riftTemplate.isInvasionRift(); if (slave != null) { this.slave = slave; slaveSpawnTemplate = slave.getSpawn(); isVolatile =
	// riftTemplate.canBeVolatile() && isWithGuards; }
	AION_UNPORTED();
}

void RVController::onDialogRequest(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

// anonymous RequestResponseHandler at RVController.java:98 (controllers.RVController$1); local responseHandler; storage: stored in ResponseRequester
// anonymous RequestResponseHandler at RVController.java:129 (controllers.RVController$2); local responseHandler; storage: stored in ResponseRequester
void RVController::onRequest(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool RVController::onAccept(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void RVController::onDespawn() {
	AION_UNPORTED();
}

int32_t RVController::getRemainTime() {
	AION_UNPORTED();
}

void RVController::syncPassed(bool invasion) {
	AION_UNPORTED();
}

std::vector<int32_t> RVController::getWorldsList(RVController& controller) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers
