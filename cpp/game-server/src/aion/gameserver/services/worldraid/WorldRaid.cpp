#include "aion/gameserver/services/worldraid/WorldRaid.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/worldraid/WorldRaidLocation.h"
#include "aion/gameserver/model/templates/worldraid/WorldRaidNpc.h"

namespace aion::gameserver::services::worldraid {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.worldraid.WorldRaid");

WorldRaid::WorldRaid(const model::templates::worldraid::WorldRaidLocation* value, bool useSpecialSpawnMsgValue, bool sendMessagesValue)
	: raidLocation(value), useSpecialSpawnMsg(useSpecialSpawnMsgValue), sendMessages(sendMessagesValue) {
}

runtime::Ref<WorldRaid> WorldRaid::create(const model::templates::worldraid::WorldRaidLocation* value, bool useSpecialSpawnMsgValue,
	bool sendMessagesValue) {
	return runtime::makeRef<WorldRaid>(value, useSpecialSpawnMsgValue, sendMessagesValue);
}

void WorldRaid::startWorldRaid() {
	AION_UNPORTED();
}

void WorldRaid::stopWorldRaid() {
	AION_UNPORTED();
}

// anonymous Runnable at WorldRaid.java:63 (services.worldraid.WorldRaid$1); argument 1 of scheduleAtFixedRate(); storage: task in WorldRaid
void WorldRaid::onWorldRaidStart() {
	AION_UNPORTED();
}

void WorldRaid::onWorldRaidFinish() {
	AION_UNPORTED();
}

void WorldRaid::scheduleBossDespawn() {
	AION_UNPORTED();
}

void WorldRaid::cancelStopRaidTask() {
	AION_UNPORTED();
}

void WorldRaid::despawnNpcs(std::initializer_list<runtime::Ptr<model::gameobjects::Npc>> npcs) {
	AION_UNPORTED();
}

void WorldRaid::despawnNpcs(const std::vector<runtime::Ptr<model::gameobjects::Npc>>& npcs) {
	AION_UNPORTED();
}

void WorldRaid::spawnAndInitRandomBoss() {
	AION_UNPORTED();
}

void WorldRaid::registerDeathObserver(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void WorldRaid::spawnAndInitMapFlag() {
	AION_UNPORTED();
}

void WorldRaid::spawnAndInitVortex() {
	AION_UNPORTED();
}

void WorldRaid::spawnAndInitMarkerSpots() {
	AION_UNPORTED();
}

void WorldRaid::broadcastMessage(network::aion::serverpackets::SM_SYSTEM_MESSAGE& msg) {
	AION_UNPORTED();
}

void WorldRaid::broadcastMessage(network::aion::serverpackets::SM_SYSTEM_MESSAGE& msg, bool forceMsg) {
	AION_UNPORTED();
}

int32_t WorldRaid::getLocationId() {
	AION_UNPORTED();
}

bool WorldRaid::isFinished() {
	AION_UNPORTED();
}

WorldRaid::~WorldRaid() = default;

} // namespace aion::gameserver::services::worldraid
