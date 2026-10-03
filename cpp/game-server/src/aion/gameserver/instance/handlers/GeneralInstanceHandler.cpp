#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"

#include <string>
#include <typeinfo>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/WorldType.h"

namespace aion::gameserver::instance::handlers {

const commons::logging::Logger GeneralInstanceHandler::log = commons::logging::LoggerFactory::getLogger("INSTANCE_LOG");

GeneralInstanceHandler::GeneralInstanceHandler(world::WorldMapInstance& instanceValue)
	: instance(instanceValue), mapId(instanceValue.getMapId()) {
}

GeneralInstanceHandler::~GeneralInstanceHandler() = default;

runtime::Ref<GeneralInstanceHandler> GeneralInstanceHandler::create(world::WorldMapInstance& instanceValue) {
	return runtime::makeRef<GeneralInstanceHandler>(instanceValue);
}

// Java GeneralInstanceHandler.java:64-68
void GeneralInstanceHandler::onLeaveInstance(model::gameobjects::player::Player& player) {
	player.getEffectController()->removeInstanceEffects();
	removeInstanceItems(player);
}

// Java GeneralInstanceHandler.java:90-93
runtime::Ptr<model::gameobjects::VisibleObject> GeneralInstanceHandler::spawn(int32_t npcId, float x, float y, float z, int8_t heading) {
	runtime::Ref<model::templates::spawns::SpawnTemplate> template_ = spawnengine::SpawnEngine::newSingleTimeSpawn(mapId, npcId, x, y, z, heading);
	return spawnengine::SpawnEngine::spawnObject(*template_, instance->getInstanceId());
}

// Java GeneralInstanceHandler.java:95-99
runtime::Ptr<model::gameobjects::VisibleObject> GeneralInstanceHandler::spawn(int32_t npcId, float x, float y, float z, int8_t heading,
	int32_t staticId) {
	runtime::Ref<model::templates::spawns::SpawnTemplate> template_ = spawnengine::SpawnEngine::newSingleTimeSpawn(mapId, npcId, x, y, z, heading);
	template_->setStaticId(staticId);
	return spawnengine::SpawnEngine::spawnObject(*template_, instance->getInstanceId());
}

// Java GeneralInstanceHandler.java:101-104
runtime::Ptr<model::gameobjects::VisibleObject> GeneralInstanceHandler::spawnAndSetRespawn(int32_t npcId, float x, float y, float z, int8_t heading,
	int32_t respawnTime) {
	runtime::Ref<model::templates::spawns::SpawnTemplate> template_ = spawnengine::SpawnEngine::newSpawn(mapId, npcId, x, y, z, heading, respawnTime);
	return spawnengine::SpawnEngine::spawnObject(*template_, instance->getInstanceId());
}

// Java GeneralInstanceHandler.java:106-108
runtime::Ptr<model::gameobjects::Npc> GeneralInstanceHandler::getNpc(int32_t npcId) {
	return instance->getNpc(npcId);
}

// Java GeneralInstanceHandler.java:110-112
void GeneralInstanceHandler::deleteAliveNpcs(std::initializer_list<int32_t> npcIds) {
	for (const runtime::Ptr<model::gameobjects::Npc>& n : instance->getNpcs(npcIds))
		n->getController().deleteIfAliveOrCancelRespawn();
}

// Java GeneralInstanceHandler.java:117-119
void GeneralInstanceHandler::sendMsg(network::aion::serverpackets::SM_SYSTEM_MESSAGE& msg) {
	sendMsg(msg, 0);
}

// Java GeneralInstanceHandler.java:124-126 (the delayed broadcast holds its own copy of the packet; Java shares the one object)
void GeneralInstanceHandler::sendMsg(network::aion::serverpackets::SM_SYSTEM_MESSAGE& msg, int32_t delay) {
	utils::PacketSendUtility::broadcastToMap(*instance, msg, delay);
}

void GeneralInstanceHandler::onDespawn(model::gameobjects::Npc& npc) {
	if (npc.getPosition()->isInstanceMap() && isBoss(npc) && !npc.isDead())
		logNpcWithReason(npc, "despawned without dying.");
}

void GeneralInstanceHandler::onDie(model::gameobjects::Npc& npc) {
	if (npc.getPosition()->isInstanceMap() && isBoss(npc))
		logNpcWithReason(npc, "was killed.");
}

void GeneralInstanceHandler::logNpcWithReason(model::gameobjects::Npc& npc, std::string_view reason) {
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> playersInside = instance->getPlayersInside();
	if (!playersInside.empty()) {
		const model::templates::world::WorldMapTemplate* worldMapTemplate = dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(mapId);
		if (worldMapTemplate == nullptr)
			throw runtime::NullPointerException("WORLD_MAPS_DATA.getTemplate(" + std::to_string(mapId) + ")");
		std::string players;
		for (const runtime::Ptr<model::gameobjects::player::Player>& p : playersInside)
			players.append(players.empty() ? "" : ", ").append(p->getName() + " (ID:" + std::to_string(p->getObjectId()) + ")");
		log.info("[{}] {} (ID:{}) {} Player(s) in instance: {}", worldMapTemplate->getName(), npc.getName(), npc.getNpcId(), reason, players);
	}
}

bool GeneralInstanceHandler::isBoss(model::gameobjects::Npc& npc) {
	using model::templates::npc::NpcRating;
	return npc.getLevel() >= 60 && (npc.getRating() == NpcRating::HERO || npc.getRating() == NpcRating::LEGENDARY);
}

// Java GeneralInstanceHandler.java:239-241
void GeneralInstanceHandler::portToStartPosition(model::gameobjects::player::Player& player) {
	throw runtime::UnsupportedOperationException(""); // Java: throw new UnsupportedOperationException();
}

float GeneralInstanceHandler::getExpMultiplier() {
	// Java GeneralInstanceHandler.java:249-251, comment and all: on retail, instances reward more exp than regular world maps. Reached on every
	// npc death since M5b-1 registered the AI (StatFunctions::calculateExperienceReward -> NpcController::doReward).
	return instance->getParent()->isInstanceType() ? 1.5f : 1.25f;
}

bool GeneralInstanceHandler::allowKiskRevive() {
	return !instance->getTemplate()->isInstance();
}

bool GeneralInstanceHandler::allowInstanceRevive() {
	// Java: `instance.getTemplate().isInstance() && getClass() != GeneralInstanceHandler.class || ... == WorldType.PANESTERRA`
	// (GeneralInstanceHandler.java:274-275). `getClass() != X.class` is an exact runtime-type test, not an instanceof: the port spells it
	// `typeid(*this) != typeid(X)`, as KnownObject::equals spells Java's `getClass() == o.getClass()` (KnownObject.cpp:32). A derived
	// instance handler - every ported instance script - therefore answers true inside an instance map, and this base class answers false.
	return instance->getTemplate()->isInstance() && typeid(*this) != typeid(GeneralInstanceHandler) ||
		instance->getTemplate()->getWorldType() == world::WorldType::PANESTERRA;
}

// Java GeneralInstanceHandler.java:278-280
bool GeneralInstanceHandler::isRestrictedToInstance(model::gameobjects::Item& item) {
	return item.getItemTemplate()->isItemRestrictedToWorld(instance->getMapId());
}

// Java GeneralInstanceHandler.java:282-292
void GeneralInstanceHandler::removeInstanceItems(model::gameobjects::player::Player& player) {
	for (const runtime::Ptr<model::gameobjects::Item>& item : player.getInventory().getItems())
		if (isRestrictedToInstance(*item))
			player.getInventory().decreaseByObjectId(item->getObjectId(), item->getItemCount());
	for (const runtime::Ptr<model::items::storage::Storage>& storage : player.getPetBags()) {
		for (const runtime::Ptr<model::gameobjects::Item>& item : storage->getItems())
			if (isRestrictedToInstance(*item))
				storage->decreaseByObjectId(item->getObjectId(), item->getItemCount());
	}
}

} // namespace aion::gameserver::instance::handlers
