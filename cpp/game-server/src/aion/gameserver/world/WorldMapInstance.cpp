#include "aion/gameserver/world/WorldMapInstance.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/StaticDoor.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/instance/StageType.h"
#include "aion/gameserver/model/team/GeneralTeam.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneClassName.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneType.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/collections/CollectionUtil.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/exceptions/DuplicateAionObjectException.h"
#include "aion/gameserver/world/zone/RegionZone.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"
#include "aion/gameserver/world/zone/ZoneService.h"

namespace aion::gameserver::world {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.world.WorldMapInstance");

namespace {

using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;

/**
 * C++ only (m5f-plan.md N-07; single-thread-synthesis.md:56, 272): the handler detachInstanceHandler installs in a destroyed instance, so the
 * instance and its real handler (GeneralInstanceHandler.instance, a Ref) no longer keep each other alive (cycles.toml
 * `WorldMapInstance.instanceHandler`). It holds no instance and is not reference counted: one leaked immortal object, whose retain()/release()
 * are no-ops. Every void body is empty; every answer is what GeneralInstanceHandler answers inside an instance map (GeneralInstanceHandler.java:
 * 87-276, and a destroyed instance is always an instance map: destroyInstance returns before detaching on any other map), without reading an
 * instance. portToStartPosition throws as the base does. A recorded deviation (docs/deviations/P4-10.md "WorldMapInstance.detachInstanceHandler"):
 * Java keeps calling the real handler of a destroyed instance until the collector takes it; the port answers with this one.
 */
class NoOpInstanceHandler final : public instance::handlers::InstanceHandler {
public:
	void onInstanceCreate() override {}
	void onInstanceDestroy() override {}
	void onPlayerLogin(Player&) override {}
	void onPlayerLogout(Player&) override {}
	void onEnterInstance(Player&) override {}
	void leaveInstance(Player&) override {}
	void onLeaveInstance(Player&) override {}
	void onOpenDoor(int32_t) override {}
	void onEnterZone(Player&, zone::ZoneInstance&) override {}
	void onLeaveZone(Player&, zone::ZoneInstance&) override {}
	void onPlayMovieEnd(Player&, int32_t) override {}
	bool onReviveEvent(Player&) override { return false; }
	void doReward(Player&) override {}
	bool onDie(Player&, Creature&) override { return false; }
	void onStopTraining(Player&) override {}
	void onDespawn(Npc&) override {}
	void onDie(Npc&) override {}
	void onSpawn(model::gameobjects::VisibleObject&) override {}
	void onChangeStage(model::instance::StageType) override {}
	void onChangeStageList(model::instance::StageList) override {}
	model::instance::StageType getStage() override { return model::instance::StageType::DEFAULT; }
	void onDropRegistered(Npc&, int32_t) override {}
	void onGather(Player&, model::gameobjects::Gatherable&) override {}
	runtime::Ptr<model::instance::instancescore::InstanceScore> getInstanceScore() override { return nullptr; }
	bool onPassFlyingRing(Player&, std::string_view) override { return false; }
	void handleUseItemFinish(runtime::Ptr<Player>, Npc&) override {}
	void onEndCastSkill(skillengine::model::Skill&) override {}
	void onAggro(Npc&) override {}
	void onStartEffect(runtime::Ptr<skillengine::model::Effect>) override {}
	void onEndEffect(skillengine::model::Effect&) override {}
	void onCreatureDetected(Npc&, Creature&) override {}
	void onSpecialEvent(Npc&) override {}
	void onBackHome(Npc&) override {}
	void portToStartPosition(Player&) override { throw runtime::UnsupportedOperationException(""); } // Java: new UnsupportedOperationException()
	bool canEnter(Player&) override { return true; }
	float getExpMultiplier() override { return 1.5f; } // GeneralInstanceHandler: 1.5f on an instance map
	float getApMultiplier() override { return 1.0f; }
	bool allowSelfReviveBySkill() override { return true; }
	bool allowSelfReviveByItem() override { return true; }
	bool allowKiskRevive() override { return false; }	  // !isInstance()
	bool allowInstanceRevive() override { return false; } // the base class inside an instance map
	void retain() const noexcept override {}
	void release() const noexcept override {}
};

NoOpInstanceHandler& noOpInstanceHandler() {
	static auto* const handler = new NoOpInstanceHandler(); // leaked immortal: destroyed instances may answer through it until process exit
	return *handler;
}

} // namespace

int32_t WorldMapInstance::regionSize() {
	static const int32_t value = configs::main::WorldConfig::WORLD_REGION_SIZE.load(); // Java: class initialization, after Config.load
	return value;
}

WorldMapInstance::WorldMapInstance(WorldMap& parentValue, int32_t instanceIdValue, int32_t maxPlayersValue,
	const std::function<runtime::Ref<instance::handlers::InstanceHandler>(WorldMapInstance&)>& instanceHandlerSupplier)
	: parent(parentValue), instanceId(instanceIdValue), maxPlayers(maxPlayersValue) {
	for (auto& [zoneName, zoneInstance] : zone::ZoneService::getInstance().getZoneInstancesByWorldId(parentValue.getMapId()))
		zones.put(zoneName, zoneInstance);
	instanceHandler.set(instanceHandlerSupplier(*this));
	// Java: initMapRegions() - called by the create() of WorldMap2DInstance/WorldMap3DInstance right after construction (class comment)
}

WorldMapInstance::~WorldMapInstance() = default;

int32_t WorldMapInstance::getMapId() {
	return getParent()->getMapId();
}

const model::templates::world::WorldMapTemplate* WorldMapInstance::getTemplate() {
	return parent->getTemplate();
}

runtime::Ptr<MapRegion> WorldMapInstance::getRegion(model::gameobjects::VisibleObject& object) {
	return getRegion(object.getX(), object.getY(), object.getZ());
}

void WorldMapInstance::addObject(model::gameobjects::VisibleObject& object) {
	if (worldMapObjects.putIfAbsent(object.getObjectId(), runtime::Ref<model::gameobjects::VisibleObject>(object))) {
		throw exceptions::DuplicateAionObjectException(object, worldMapObjects.get(object.getObjectId()));
	}
	if (auto* npc = dynamic_cast<model::gameobjects::Npc*>(&object)) {
		worldMapNpcs.put(object.getObjectId(), runtime::Ref<model::gameobjects::Npc>(*npc));
		runtime::Ref<model::templates::quest::QuestNpc> qNpc =
			questEngine::QuestEngine::getInstance().getQuestNpc(object.getObjectTemplate()->getTemplateId());
		if (qNpc) {
			bool updateNearbyQuests = false;
			for (int32_t id : qNpc->getOnQuestStart()) {
				if (questIds.add(id)) {
					updateNearbyQuests = true;
				}
			}
			if (updateNearbyQuests && !updateNearbyQuestsTask.get()) { // delayed with null check to prevent packet spam on multispawns (bases, siege, ...)
				// task lambda WorldMapInstance@L125:72: pin {this}
				updateNearbyQuestsTask.set(utils::ThreadPoolManager::getInstance().schedule({this}, [this] {
					updateNearbyQuestsTask.set(runtime::Ptr<runtime::Future>());
					forEachPlayer([](model::gameobjects::player::Player& player) { player.getController().updateNearbyQuests(); });
				}, 1500));
			}
		}
	}
	if (auto* player = dynamic_cast<model::gameobjects::player::Player*>(&object)) {
		if (getParent()->isFlightAllowed())
			player->setInsideZoneType(model::templates::zone::ZoneType::FLY);
		worldMapPlayers.put(object.getObjectId(), runtime::Ref<model::gameobjects::player::Player>(*player));
	}
}

void WorldMapInstance::removeObject(model::gameobjects::AionObject& object) {
	worldMapObjects.remove(object.getObjectId());
	if (auto* player = dynamic_cast<model::gameobjects::player::Player*>(&object)) {
		lastPlayerLeaveTime.set(commons::utils::currentTimeMillis());
		if (getParent()->isFlightAllowed()) {
			player->unsetInsideZoneType(model::templates::zone::ZoneType::FLY);
			// necessary for fly maps like the abyss (they don't have FlyZones, so no FlyZoneInstance.onLeave() is called)
			player->getController().onLeaveFlyArea();
		}
		worldMapPlayers.remove(object.getObjectId());
	} else if (dynamic_cast<model::gameobjects::Npc*>(&object) != nullptr) {
		worldMapNpcs.remove(object.getObjectId());
	}
}

int32_t WorldMapInstance::getPlayerCount() {
	return worldMapPlayers.size();
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> WorldMapInstance::getPlayersInside() {
	return worldMapPlayers.values().toVector();
}

runtime::Ptr<model::gameobjects::player::Player> WorldMapInstance::getPlayer(int32_t objId) {
	return worldMapPlayers.get(objId);
}

runtime::Ptr<model::gameobjects::VisibleObject> WorldMapInstance::getObject(int32_t objId) {
	return worldMapObjects.get(objId);
}

runtime::Ptr<model::gameobjects::Npc> WorldMapInstance::getNpc(int32_t npcId) {
	for (runtime::Ptr<model::gameobjects::Npc> npc : worldMapNpcs.values()) {
		if (npc && npc->getNpcId() == npcId)
			return npc;
	}
	return nullptr;
}

std::vector<runtime::Ptr<model::gameobjects::Npc>> WorldMapInstance::getNpcs(std::initializer_list<int32_t> npcIds) {
	std::vector<runtime::Ptr<model::gameobjects::Npc>> npcs;
	forEachNpc([&npcs, npcIds](model::gameobjects::Npc& npc) {
		for (int32_t npcId : npcIds) {
			if (npc.getNpcId() == npcId)
				npcs.emplace_back(npc);
		}
	});
	return npcs;
}

std::vector<runtime::Ptr<model::gameobjects::Npc>> WorldMapInstance::getNpcs() {
	std::vector<runtime::Ptr<model::gameobjects::Npc>> npcs;
	forEachNpc([&npcs](model::gameobjects::Npc& npc) { npcs.emplace_back(npc); });
	return npcs;
}

runtime::Ptr<model::gameobjects::VisibleObject> WorldMapInstance::getObjectByStaticId(int32_t staticId) {
	for (runtime::Ptr<model::gameobjects::VisibleObject> o : worldMapObjects.values()) {
		if (o && o->getSpawn() && o->getSpawn()->getStaticId() == staticId)
			return o;
	}
	return nullptr;
}

void WorldMapInstance::detachInstanceHandler() {
	// C++ only (cycles.toml `WorldMapInstance.instanceHandler`, called by InstanceService::destroyInstance after onInstanceDestroy())
	instanceHandler.set(runtime::Ref<instance::handlers::InstanceHandler>(noOpInstanceHandler()));
}

void WorldMapInstance::releaseRegisteredTeam() noexcept {
	registeredTeam.set(runtime::Ptr<model::team::GeneralTeam>());
}

bool WorldMapInstance::isBeginnerInstance() {
	// Java: if (parent == null) return false; (parent is final and never null in C++)
	if (parent->getTemplate()->isInstance()) {
		// TODO: check Karamatis and Ataxiar for exception in FastTrack ?
		// return parent.getTemplate().getBeginnerTwinCount() > 0;
		return false;
	}

	int32_t twinCount = parent->getTemplate()->getTwinCount();
	if (twinCount == 0)
		twinCount = 1;
	return getInstanceId() > twinCount;
}

void WorldMapInstance::registerTeam(model::team::GeneralTeam& team) {
	if (registeredTeam.get())
		throw runtime::IllegalStateException("A team for instance " + std::to_string(instanceId) + " of map " + std::to_string(getMapId()) +
			" is already registered");
	registeredTeam.set(runtime::Ptr<model::team::GeneralTeam>(team));
	register_(team.getTeamId());
}

void WorldMapInstance::register_(int32_t objectId) {
	registeredObjects.add(objectId);
}

int32_t WorldMapInstance::getRegisteredCount() {
	return registeredObjects.size();
}

bool WorldMapInstance::isRegistered(int32_t objectId) {
	return registeredObjects.contains(objectId);
}

void WorldMapInstance::setEmptyInstanceTask(runtime::Ptr<runtime::Future> value) {
	emptyInstanceTask.set(value);
}

void WorldMapInstance::setStartPos(runtime::Ptr<WorldPosition> value) {
	startPos.set(value);
}

std::vector<runtime::Ptr<zone::ZoneInstance>> WorldMapInstance::filterZones(int32_t mapId, int32_t regionId, float startX, float startY, float minZ,
	float maxZ) {
	runtime::Ref<zone::RegionZone> regionZone = zone::RegionZone::create(startX, startY, minZ, maxZ);
	std::vector<runtime::Ptr<zone::ZoneInstance>> result;
	for (runtime::Ptr<zone::ZoneInstance> zoneInstance : zones.values()) {
		if (zoneInstance->getAreaTemplate()->intersectsRectangle(*regionZone)) {
			result.push_back(zoneInstance);
			continue;
		}
		if (zoneInstance->getZoneTemplate()->getZoneType() == model::templates::zone::ZoneClassName::DUMMY)
			log.error("Region " + std::to_string(regionId) + " should intersect with whole map zone!!! (map=" + std::to_string(mapId) + ")");
	}
	return result;
}

bool WorldMapInstance::isInsideZone(model::gameobjects::VisibleObject& object, const zone::ZoneName* zoneName) {
	runtime::Ptr<zone::ZoneInstance> zoneTemplate = zones.get(zoneName);
	return zoneTemplate && isInsideZone(*object.getPosition(), zoneName);
}

std::vector<const zone::ZoneName*> WorldMapInstance::getZoneNames() {
	std::vector<const zone::ZoneName*> names;
	for (const zone::ZoneName* zoneName : zones.keySet())
		names.push_back(zoneName);
	return names;
}

bool WorldMapInstance::isInsideZone(WorldPosition& pos, const zone::ZoneName* zoneName) {
	runtime::Ptr<MapRegion> mapRegion = getRegion(pos.getX(), pos.getY(), pos.getZ());
	return mapRegion->isInsideZone(zoneName, pos.getX(), pos.getY(), pos.getZ());
}

bool WorldMapInstance::isFull() {
	return maxPlayers > 0 && getPlayerCount() >= maxPlayers;
}

void WorldMapInstance::setDoorState(int32_t staticId, bool open) {
	for (runtime::Ptr<model::gameobjects::VisibleObject> v : worldMapObjects.values()) {
		if (auto staticDoor = runtime::as<model::gameobjects::StaticDoor>(v); staticDoor && v->getSpawn()->getStaticId() == staticId) {
			staticDoor->setOpen(open);
			return;
		}
	}
	log.warn("Door (ID: " + std::to_string(staticId) + ") doesn't exist", runtime::Exception("java.lang.RuntimeException"));
}

runtime::JavaIterator<runtime::Ptr<model::gameobjects::VisibleObject>> WorldMapInstance::iterator() {
	return worldMapObjects.values().iterator();
}

runtime::SnapshotIterator<runtime::Ptr<model::gameobjects::VisibleObject>> WorldMapInstance::begin() {
	return runtime::SnapshotIterator<runtime::Ptr<model::gameobjects::VisibleObject>>(
		std::make_shared<const std::vector<runtime::Ptr<model::gameobjects::VisibleObject>>>(worldMapObjects.values().toVector()));
}

void WorldMapInstance::forEachNpc(const std::function<void(model::gameobjects::Npc&)>& consumer) {
	utils::collections::CollectionUtil::forEach(worldMapNpcs.values(), [&consumer](const runtime::Ptr<model::gameobjects::Npc>& npc) { consumer(*npc); });
}

void WorldMapInstance::forEachPlayer(const std::function<void(model::gameobjects::player::Player&)>& consumer) {
	utils::collections::CollectionUtil::forEach(worldMapPlayers.values(),
		[&consumer](const runtime::Ptr<model::gameobjects::player::Player>& player) { consumer(*player); });
}

void WorldMapInstance::forEachObject(const std::function<void(model::gameobjects::VisibleObject&)>& consumer) {
	utils::collections::CollectionUtil::forEach(worldMapObjects.values(),
		[&consumer](const runtime::Ptr<model::gameobjects::VisibleObject>& object) { consumer(*object); });
}

void WorldMapInstance::forEachDoor(const std::function<void(model::gameobjects::StaticDoor&)>& consumer) {
	utils::collections::CollectionUtil::forEach(worldMapObjects.values(), [&consumer](const runtime::Ptr<model::gameobjects::VisibleObject>& o) {
		if (auto staticDoor = runtime::as<model::gameobjects::StaticDoor>(o))
			consumer(*staticDoor);
	});
}

std::string WorldMapInstance::toString() {
	return "WorldMapInstance " + std::to_string(getMapId()) + " [" + std::to_string(instanceId) + "]";
}

} // namespace aion::gameserver::world
