#include "aion/gameserver/model/base/Base.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/base/BaseLocation.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::model::base {

Base::Base(BaseLocation& value)
	: bLoc(runtime::Ref<BaseLocation>(value)), id() {
	// Java: this.id = bLoc.getId()
	AION_UNPORTED();
}

void Base::start() {
	AION_UNPORTED();
}

void Base::stop() {
	AION_UNPORTED();
}

void Base::handleStart() {
	AION_UNPORTED();
}

void Base::handleStop() {
	AION_UNPORTED();
}

void Base::despawnAllNpcs() {
	AION_UNPORTED();
}

void Base::despawnByHandlerType(spawnengine::SpawnHandlerType type) {
	AION_UNPORTED();
}

void Base::despawnNpcs(spawnengine::SpawnHandlerType type) {
	AION_UNPORTED();
}

bool Base::isSpawnForCurrentBase(templates::spawns::SpawnTemplate& spawnTemplate, spawnengine::SpawnHandlerType type) {
	AION_UNPORTED();
}

// lambda at Base.java:98 (fieldmap key base.Base@L98:64)
void Base::scheduleOutriderSpawn() {
	AION_UNPORTED();
}

// lambda at Base.java:109 (fieldmap key base.Base@L109:60)
void Base::scheduleBossSpawn() {
	AION_UNPORTED();
}

// lambda at Base.java:124 (fieldmap key base.Base@L124:58)
void Base::scheduleAssault() {
	AION_UNPORTED();
}

BaseOccupier Base::chooseAssaultRace() {
	AION_UNPORTED();
}

// lambda at Base.java:148 (fieldmap key base.Base@L148:65)
void Base::scheduleAssaultDespawn() {
	AION_UNPORTED();
}

void Base::despawnAssaulter() {
	AION_UNPORTED();
}

void Base::spawnBySpawnHandler(spawnengine::SpawnHandlerType type, BaseOccupier occupier) {
	AION_UNPORTED();
}

BaseOccupier Base::getOccupier(runtime::Ptr<gameobjects::Creature> bossKiller) {
	AION_UNPORTED();
}

network::aion::serverpackets::SM_SYSTEM_MESSAGE Base::getBossSpawnMsg() {
	AION_UNPORTED();
}

network::aion::serverpackets::SM_SYSTEM_MESSAGE Base::getAssaultMsg() {
	AION_UNPORTED();
}

void Base::cancelTask(std::initializer_list<runtime::FutureRef> tasks) {
	AION_UNPORTED();
}

int32_t Base::getWorldId() {
	AION_UNPORTED();
}

BaseOccupier Base::getOccupier() {
	AION_UNPORTED();
}

bool Base::isStarted() {
	AION_UNPORTED();
}

bool Base::isStopped() {
	AION_UNPORTED();
}

bool Base::isUnderAssault() {
	AION_UNPORTED();
}

Base::~Base() = default;

} // namespace aion::gameserver::model::base
