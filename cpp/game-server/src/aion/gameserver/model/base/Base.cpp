#include "aion/gameserver/model/base/Base.h"

#include <algorithm>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/model/base/BaseException.h"
#include "aion/gameserver/model/base/BaseLocation.h"
#include "aion/gameserver/model/base/BaseOccupier.h"
#include "aion/gameserver/model/base/BaseOccupierInfo.h"
#include "aion/gameserver/model/base/BaseType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/templates/base/BaseTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/basespawns/BaseSpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/BaseService.h"
#include "aion/gameserver/services/RespawnService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/SpawnHandlerType.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::model::base {

using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using spawnengine::SpawnHandlerType;

// Java Base.java:45-48
Base::Base(BaseLocation& value) : bLoc(runtime::Ref<BaseLocation>(value)), id(value.getId()) {
}

// Java Base.java:50-55
void Base::start() {
	if (isStarted_.compareAndSet(false, true))
		handleStart();
	else
		throw BaseException("Attempt to start Base twice! ID:" + std::to_string(id));
}

// Java Base.java:57-62
void Base::stop() {
	if (isStopped_.compareAndSet(false, true))
		handleStop();
	else
		throw BaseException("Attempt to stop Base twice! ID:" + std::to_string(id));
}

// Java Base.java:64-70
void Base::handleStart() {
	spawnBySpawnHandler(SpawnHandlerType::FLAG, getOccupier());
	spawnBySpawnHandler(SpawnHandlerType::MERCHANT, getOccupier());
	spawnBySpawnHandler(SpawnHandlerType::SENTINEL, getOccupier());
	scheduleOutriderSpawn();
	scheduleBossSpawn();
}

// Java Base.java:72-75
void Base::handleStop() {
	cancelTask({assaultTask.get(), assaultDespawnTask.get(), bossSpawnTask.get(), outriderSpawnTask.get()});
	despawnAllNpcs();
}

// Java Base.java:77-79
void Base::despawnAllNpcs() {
	despawnNpcs(std::nullopt);
}

// Java Base.java:81-83
void Base::despawnByHandlerType(SpawnHandlerType type) {
	despawnNpcs(type);
}

// Java Base.java:85-91
void Base::despawnNpcs(std::optional<SpawnHandlerType> type) {
	world::World::getInstance().getWorldMap(getLocation()->getTemplate()->getWorldId())->forEachObject([this, type](gameobjects::VisibleObject& o) {
		if (isSpawnForCurrentBase(o.getSpawn(), type))
			o.getController().deleteIfAliveOrCancelRespawn();
	});
	services::RespawnService::cancelRespawns(
		[this, type](templates::spawns::SpawnTemplate& spawnTemplate) { return isSpawnForCurrentBase(runtime::Ptr<templates::spawns::SpawnTemplate>(spawnTemplate), type); });
}

// Java Base.java:93-95
bool Base::isSpawnForCurrentBase(runtime::Ptr<templates::spawns::SpawnTemplate> spawnTemplate, std::optional<SpawnHandlerType> type) {
	if (!spawnTemplate)
		return false; // Java: null instanceof BaseSpawnTemplate is false
	runtime::Ptr<templates::spawns::basespawns::BaseSpawnTemplate> spawn = runtime::as<templates::spawns::basespawns::BaseSpawnTemplate>(spawnTemplate);
	return spawn && spawn->getId() == id && (!type || spawn->getHandlerType() == type);
}

// Java Base.java:97-103; lambda at Base.java:98 (the outrider spawn task, pinning this)
void Base::scheduleOutriderSpawn() {
	outriderSpawnTask.set(utils::ThreadPoolManager::getInstance().schedule(this, [this] {
		if (isStopped_.get() || getNpcSpawnDelay() == 0)
			return;
		spawnBySpawnHandler(SpawnHandlerType::OUTRIDER, getOccupier());
	}, getNpcSpawnDelay()));
}

// Java Base.java:105-118; lambda at Base.java:109 (the boss spawn task, pinning this)
void Base::scheduleBossSpawn() {
	if (bLoc->getOccupier() == BaseOccupier::PEACE)
		return; // Peace does not include any boss or the possibility to capture it

	bossSpawnTask.set(utils::ThreadPoolManager::getInstance().schedule(this, [this] {
		if (isStopped_.get())
			return;
		spawnBySpawnHandler(SpawnHandlerType::BOSS, getOccupier());
		std::optional<SM_SYSTEM_MESSAGE> bossSpawnMsg = getBossSpawnMsg();
		if (bossSpawnMsg)
			utils::PacketSendUtility::broadcastToMap(*flag.get()->getPosition()->getWorldMapInstance(), *bossSpawnMsg);
		scheduleAssault();
	}, getBossSpawnDelay()));
}

// Java Base.java:120-139; lambda at Base.java:124 (the assault task, pinning this)
void Base::scheduleAssault() {
	if (bLoc->getType() == BaseType::PANESTERRA_FACTION_CAMP || bLoc->getType() == BaseType::PANESTERRA_ARTIFACT)
		return; // No assault for those two

	assaultTask.set(utils::ThreadPoolManager::getInstance().schedule(this, [this] {
		if (isStopped_.get())
			return;
		if (flag.get()->getPosition()->isMapRegionActive()) {
			spawnBySpawnHandler(SpawnHandlerType::ATTACKER, chooseAssaultRace());
			std::optional<SM_SYSTEM_MESSAGE> assaultMsg = getAssaultMsg();
			if (assaultMsg)
				utils::PacketSendUtility::broadcastToMap(*flag.get()->getPosition()->getWorldMapInstance(), *assaultMsg);
			scheduleAssaultDespawn();
		} else {
			if (commons::utils::Rnd::chance() < 20)
				services::BaseService::getInstance().capture(id, chooseAssaultRace());
			scheduleAssault();
		}
	}, getAssaultDelay()));
}

// Java Base.java:141-145
BaseOccupier Base::chooseAssaultRace() {
	std::vector<BaseOccupier> coll{BaseOccupier::ASMODIANS, BaseOccupier::ELYOS, BaseOccupier::BALAUR};
	// Java List.remove(Object): the first equal element, if any
	if (auto it = std::find(coll.begin(), coll.end(), getOccupier()); it != coll.end())
		coll.erase(it);
	return *commons::utils::Rnd::get(coll);
}

// Java Base.java:147-154; lambda at Base.java:148 (the assault despawn task, pinning this)
void Base::scheduleAssaultDespawn() {
	assaultDespawnTask.set(utils::ThreadPoolManager::getInstance().schedule(this, [this] {
		if (isStopped_.get())
			return;
		despawnAssaulter();
		scheduleAssault();
	}, getAssaultDespawnDelay()));
}

// Java Base.java:156-160
void Base::despawnAssaulter() {
	for (const runtime::Ptr<gameobjects::Npc>& npc : assaulter.snapshot())
		npc->getController().deleteIfAliveOrCancelRespawn();
	assaulter.clear();
}

// Java Base.java:162-194
void Base::spawnBySpawnHandler(SpawnHandlerType type, BaseOccupier occupier) {
	runtime::Ptr<gameobjects::Npc> boss;
	const std::vector<runtime::Ref<templates::spawns::SpawnGroup>>* groups = dataholders::DataManager::SPAWNS_DATA->getBaseSpawnsByLocId(id);
	if (groups == nullptr)
		throw runtime::NullPointerException("base spawns of " + std::to_string(id)); // Java: the for-each over null
	for (const runtime::Ref<templates::spawns::SpawnGroup>& group : *groups) {
		if (group->getHandlerType() != type)
			continue;
		for (const runtime::Ptr<templates::spawns::SpawnTemplate>& template_ : group->getSpawnTemplates().snapshot()) {
			if (runtime::cast<templates::spawns::basespawns::BaseSpawnTemplate>(template_)->getOccupier() != occupier)
				continue;
			runtime::Ptr<gameobjects::Npc> npc = runtime::cast<gameobjects::Npc>(spawnengine::SpawnEngine::spawnObject(*template_, 1));
			if (!npc)
				throw BaseException("Npc " + std::to_string(template_->getNpcId()) + " could not be spawned at base " + std::to_string(id));
			switch (type) {
				case SpawnHandlerType::ATTACKER:
					assaulter.add(runtime::Ref<gameobjects::Npc>(npc));
					break;
				case SpawnHandlerType::BOSS:
					if (boss)
						throw BaseException("Tried to spawn boss twice at base " + std::to_string(id));
					boss = npc;
					break;
				case SpawnHandlerType::FLAG:
					if (flag.get())
						throw BaseException("Tried to spawn flag twice at base " + std::to_string(id));
					flag.set(runtime::Ref<gameobjects::Npc>(npc));
					break;
				default:
					break;
			}
		}
	}
	if (type == SpawnHandlerType::BOSS && !boss)
		throw BaseException("No boss found for base! ID: " + std::to_string(id));
	if (type == SpawnHandlerType::FLAG && !flag.get())
		throw BaseException("No flag found for base! ID: " + std::to_string(id));
}

// Java Base.java:196-198
BaseOccupier Base::getOccupier(runtime::Ptr<gameobjects::Creature> bossKiller) {
	return !bossKiller ? getLocation()->getTemplate()->getDefaultOccupier() : findBy(bossKiller->getRace());
}

// Java Base.java:200-217
std::optional<SM_SYSTEM_MESSAGE> Base::getBossSpawnMsg() {
	switch (id) {
		case 6101: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_CHIEF_V01();
		case 6102: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_CHIEF_V02();
		case 6103: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_CHIEF_V03();
		case 6104: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_CHIEF_V04();
		case 6105: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_CHIEF_V05();
		case 6106: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_CHIEF_V06();
		case 6107: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_CHIEF_V07();
		case 6108: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_CHIEF_V08();
		case 6109: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_CHIEF_V09();
		case 6110: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_CHIEF_V10();
		case 6111: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_CHIEF_V11();
		case 6112: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_CHIEF_V12();
		case 6113: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_CHIEF_V13();
		default: return std::nullopt;
	}
}

// Java Base.java:219-236
std::optional<SM_SYSTEM_MESSAGE> Base::getAssaultMsg() {
	switch (id) {
		case 6101: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_KILLER_V01();
		case 6102: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_KILLER_V02();
		case 6103: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_KILLER_V03();
		case 6104: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_KILLER_V04();
		case 6105: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_KILLER_V05();
		case 6106: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_KILLER_V06();
		case 6107: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_KILLER_V07();
		case 6108: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_KILLER_V08();
		case 6109: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_KILLER_V09();
		case 6110: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_KILLER_V10();
		case 6111: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_KILLER_V11();
		case 6112: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_KILLER_V12();
		case 6113: return SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_KILLER_V13();
		default: return std::nullopt;
	}
}

// Java Base.java:242-247
void Base::cancelTask(std::initializer_list<runtime::FutureRef> tasks) {
	for (const runtime::FutureRef& task : tasks) {
		if (task && !task->isDone())
			task->cancel(true);
	}
}

// Java Base.java:257-259
int32_t Base::getWorldId() {
	return bLoc->getWorldId();
}

// Java Base.java:261-263
BaseOccupier Base::getOccupier() {
	return bLoc->getOccupier();
}

// Java Base.java:265-267
bool Base::isStarted() {
	return isStarted_.get();
}

// Java Base.java:269-271
bool Base::isStopped() {
	return isStopped_.get();
}

// Java Base.java:273-275
bool Base::isUnderAssault() {
	return !assaulter.isEmpty();
}

Base::~Base() = default;

} // namespace aion::gameserver::model::base
