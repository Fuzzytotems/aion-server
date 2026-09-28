// C++-only leak census holder probe of the game layer. See WorldLeakProbe.h.

#include "aion/gameserver/world/WorldLeakProbe.h"

#include <algorithm>
#include <exception>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SiegeLocationData.h"
#include "aion/gameserver/dataholders/WalkerData.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/spawnengine/ClusteredNpc.h"
#include "aion/gameserver/spawnengine/InstanceWalkerFormations.h"
#include "aion/gameserver/spawnengine/WalkerFormationsCache.h"
#include "aion/gameserver/spawnengine/WalkerGroup.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMap3DInstance.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::world {

const char* const WorldLeakProbe::SEARCHED =
	"World.allObjects; the target and known list of every world object; the aggro list, casting skill and effects of every creature; "
	"Player.postman; House.spawns; worldMapObjects, worldMapNpcs and the object and zone maps of every region of every instance of every world "
	"map; the WalkerGroup of the npc's route and of its own Npc.walkerGroup (WalkerGroup.members); SiegeLocation.creatures";

const char* const WorldLeakProbe::NOT_SEARCHED =
	"InstanceWalkerFormations.groupedSpawnObjects, formationVariants and walkerVariants, MoveTaskManager.movingCreatures, "
	"PlayerMoveTaskManager.movingPlayers, TemporarySpawnEngine.spawnedObjects, Town.spawnedNpcs, Base.flag and assaulter and the npcs of a "
	"WorldRaid (private in the port, header request m5b3-leak-h03); the other fields of world objects (e.g. SummonedObject.creator, "
	"Player.summon, Player.kisk); objects outside the world (another removed or leaked object, a walker respawn that cacheWalkerCandidate "
	"cached and never spawned); the ObserveController observers of every creature (what an observer captures cannot be searched by identity); "
	"pending tasks (the census names those that pin the object); the other service singletons";

namespace {

using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;

/** C++-only class: no Java logger; the name follows the package of the class */
const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.world.WorldLeakProbe"));
	return *logger;
}

/** one object the probe looks for, and what it found */
struct Target {
	VisibleObject* object;
	/** `object` as a creature, or nullptr: only creatures enter zones, aggro lists and skills */
	Creature* creature;
	std::vector<std::string>* holders;
	/** the zones already reported for it (a zone is listed by every region it overlaps) */
	std::unordered_set<zone::ZoneInstance*> zones;
};

/** one effect of a creature, as the probe compares it */
struct EffectParties {
	int32_t skillId;
	Creature* effector;
	Creature* effected;
};

/**
 * the references `other` holds to each of `targets` through its own fields and parts; every snapshot of `other` (aggro list, casting skill,
 * effects) is taken once for all targets, and `other` is named only for a reference found
 */
void findInObject(VisibleObject& other, std::span<Target> targets) {
	std::optional<std::string> otherName;
	auto name = [&other, &otherName]() -> const std::string& {
		if (!otherName)
			otherName = other.toString();
		return *otherName;
	};
	auto* creature = dynamic_cast<Creature*>(&other);
	bool creatureTargets = std::ranges::any_of(targets, [](const Target& target) { return target.creature != nullptr; });
	VisibleObject* otherTarget = other.getTarget().get();
	std::vector<Creature*> attackers;
	runtime::Ptr<skillengine::model::Skill> skill;
	std::vector<Creature*> effectedBySkill;
	std::vector<EffectParties> effects;
	if (creature != nullptr && creatureTargets) {
		for (runtime::Ptr<controllers::attack::AggroInfo> info : creature->getAggroList().stream())
			attackers.push_back(info->getAttacker().get());
		skill = creature->getCastingSkill();
		if (skill) {
			for (runtime::Ptr<Creature> effected : skill->getEffectedList())
				effectedBySkill.push_back(effected.get());
		}
		if (runtime::Ptr<controllers::effect::EffectController> effectController = creature->getEffectController()) {
			for (runtime::Ptr<skillengine::model::Effect> effect : effectController->getAllEffects())
				effects.push_back(EffectParties{effect->getSkillId(), effect->getEffector().get(), effect->getEffected().get()});
		}
	}
	auto* player = dynamic_cast<model::gameobjects::player::Player*>(&other);
	VisibleObject* postman = player != nullptr ? player->getPostman().get() : nullptr;
	auto* house = player == nullptr ? dynamic_cast<model::house::House*>(&other) : nullptr;
	std::vector<VisibleObject*> houseSpawns;
	if (house != nullptr)
		houseSpawns = {house->getButler().get(), house->getRelationshipCrystal().get(), house->getCurrentSign().get()};

	for (Target& target : targets) {
		std::vector<std::string>& holders = *target.holders;
		if (&other == target.object) {
			holders.push_back("World.allObjects");
			continue;
		}
		if (otherTarget == target.object)
			holders.push_back("the target of " + name());
		try {
			if (other.getKnownList().getObject(target.object->getObjectId()).get() == target.object)
				holders.push_back("the known list of " + name());
		} catch (const std::exception&) {
			// an object without a known list
		}
		if (postman == target.object)
			holders.push_back("Player.postman of " + name());
		if (std::ranges::find(houseSpawns, target.object) != houseSpawns.end())
			holders.push_back("House.spawns of " + name());
		if (creature == nullptr || target.creature == nullptr)
			continue;
		for (Creature* attacker : attackers) {
			if (attacker == target.creature)
				holders.push_back("the aggro list of " + name());
		}
		if (skill) {
			std::string what = "the casting skill " + std::to_string(skill->getSkillId()) + " of " + name();
			if (skill->getEffector().get() == target.creature)
				holders.push_back(what + " (Skill.effector)");
			if (skill->getFirstTarget().get() == target.creature)
				holders.push_back(what + " (Skill.firstTarget)");
			for (Creature* effected : effectedBySkill) {
				if (effected == target.creature)
					holders.push_back(what + " (Skill.effectedList)");
			}
		}
		for (const EffectParties& effect : effects) {
			if (effect.effector != target.creature && effect.effected != target.creature)
				continue;
			std::string what = "effect " + std::to_string(effect.skillId) + " on " + name();
			if (effect.effector == target.creature)
				holders.push_back(what + " (Effect.effector)");
			if (effect.effected == target.creature)
				holders.push_back(what + " (Effect.effected)");
		}
	}
}

/** every map region of the instance, found through getRegion on the region grid (the instance does not list them) */
std::vector<MapRegion*> regionsOf(WorldMapInstance& instance) {
	int32_t size = WorldMapInstance::regionSize();
	int32_t worldSize = instance.getParent()->getWorldSize();
	int32_t maxZ = dynamic_cast<WorldMap3DInstance*>(&instance) != nullptr ? worldSize : 0;
	std::vector<MapRegion*> regions;
	std::unordered_set<MapRegion*> seen;
	for (int32_t x = 0; x <= worldSize; x += size) {
		for (int32_t y = 0; y <= worldSize; y += size) {
			for (int32_t z = 0; z <= maxZ; z += size) {
				runtime::Ptr<MapRegion> region = instance.getRegion(static_cast<float>(x + 1), static_cast<float>(y + 1), static_cast<float>(z + 1));
				if (region && seen.insert(region.get()).second)
					regions.push_back(region.get());
			}
		}
	}
	return regions;
}

/** the containers of one map instance: its object maps, and the object and zone maps of its regions */
void findInMapInstance(WorldMapInstance& instance, std::vector<Target>& targets) {
	std::string of = " of " + instance.toString();
	for (Target& target : targets) {
		if (instance.getObject(target.object->getObjectId()).get() == target.object)
			target.holders->push_back("WorldMapInstance.worldMapObjects" + of);
	}
	for (runtime::Ptr<Npc> npc : instance.getNpcs()) {
		for (Target& target : targets) {
			if (npc.get() == target.object)
				target.holders->push_back("WorldMapInstance.worldMapNpcs" + of);
		}
	}
	for (MapRegion* region : regionsOf(instance)) {
		for (Target& target : targets) {
			if (region->getObjects().get(target.object->getObjectId()).get() == target.object)
				target.holders->push_back("MapRegion.objects of region " + std::to_string(region->getRegionId()) + of);
			if (target.creature == nullptr)
				continue;
			// findZones matches the object id (ZoneInstance.creatures is keyed by it); forEach confirms the entry is this very creature
			for (runtime::Ptr<zone::ZoneInstance> zone : region->findZones(*target.creature)) {
				if (!target.zones.insert(zone.get()).second)
					continue;
				bool same = false;
				zone->forEach([&target, &same](Creature& creature) { same = same || &creature == target.creature; });
				if (same)
					target.holders->push_back("ZoneInstance.creatures of zone " + zone->getZoneTemplate()->getName()->name() + of);
			}
		}
	}
}

/** every instance of every world map (World creates one WorldMap per template of world_maps.xml) */
void findInMapInstances(std::vector<Target>& targets) {
	const dataholders::WorldMapsData* maps = dataholders::DataManager::WORLD_MAPS_DATA.get();
	if (maps == nullptr)
		return;
	for (const model::templates::world::WorldMapTemplate* mapTemplate : *maps) {
		runtime::Ptr<WorldMap> map = World::getInstance().getWorldMap(mapTemplate->getMapId());
		if (!map)
			continue;
		for (runtime::Ptr<WorldMapInstance> instance : *map) {
			for (Target& target : targets)
				target.zones.clear(); // zone instances belong to one map instance
			findInMapInstance(*instance, targets);
		}
	}
}

/**
 * ClusteredNpc.npc of a member of the walker npc's group: the group of its route in its instance (WalkerFormator.processClusteredNpc's lookup;
 * processClusteredNpc created that instance's formations when it spawned the npc, so the lookup creates nothing - unless onInstanceDestroy
 * dropped them since, see WorldLeakProbe::findHolders) and the group of its own Npc.walkerGroup (set again by WalkerGroup.form after
 * LogoutBreakers D5 cut it). Java keeps the same references and reads them (WalkerGroup.targetReached, respawn, setStep): a dead member stays
 * until a respawn replaces it.
 */
void findInWalkerGroups(Target& target) {
	auto* npc = dynamic_cast<Npc*>(target.object);
	if (npc == nullptr)
		return;
	std::vector<spawnengine::WalkerGroup*> checked;
	auto check = [&](runtime::Ptr<spawnengine::WalkerGroup> group, const std::string& via) {
		if (!group || std::ranges::find(checked, group.get()) != checked.end())
			return;
		checked.push_back(group.get());
		runtime::Ptr<spawnengine::ClusteredNpc> member = group->getClusterData(*npc);
		if (member && member->getNpc().get() == npc)
			target.holders->push_back("ClusteredNpc.npc of a member of WalkerGroup " + member->getWalkTemplate()->getRouteId() + " (" + via +
				" -> WalkerGroup.members; Java's group keeps and reads it too, until a respawn replaces a dead member)");
	};
	runtime::Ptr<model::templates::spawns::SpawnTemplate> spawn = npc->getSpawn();
	std::optional<std::string> walkerId = spawn ? spawn->getWalkerId() : std::nullopt;
	if (walkerId && dataholders::DataManager::WALKER_DATA) {
		const model::templates::walker::WalkerTemplate* walker = dataholders::DataManager::WALKER_DATA->getWalkerTemplate(*walkerId);
		if (walker != nullptr && walker->getPool() >= 2) {
			runtime::Ptr<spawnengine::InstanceWalkerFormations> formations =
				spawnengine::WalkerFormationsCache::getInstanceFormations(spawn->getWorldId(), npc->getInstanceId());
			check(formations->getSpawnWalkerGroup(*walkerId), "WalkerFormationsCache -> InstanceWalkerFormations.walkFormations");
		}
	}
	check(npc->getWalkerGroup(), "its own Npc.walkerGroup, a group <-> npc cycle");
}

/** SiegeLocation.creatures (SiegeService.locations are the SiegeLocationData objects) */
void findInSiegeLocations(std::vector<Target>& targets) {
	const dataholders::SiegeLocationData* data = dataholders::DataManager::SIEGE_LOCATION_DATA.get();
	if (data == nullptr)
		return;
	for (const auto& [id, location] : data->getSiegeLocations()) {
		location->forEachCreature([&targets, id](Creature& creature) {
			for (Target& target : targets) {
				if (&creature == target.creature)
					target.holders->push_back("SiegeLocation.creatures of siege location " + std::to_string(id));
			}
		});
	}
}

/** runs one search step, noting a step that failed as a holder line of its targets instead of losing the other steps */
template <class F>
void guarded(const char* step, std::span<Target> targets, F&& search) {
	try {
		search();
	} catch (const std::exception& e) {
		for (Target& target : targets)
			target.holders->push_back(std::string("? (") + step + " could not be searched: " + e.what() + ")");
	}
}

} // namespace

std::vector<std::vector<std::string>> WorldLeakProbe::findHolders(std::span<VisibleObject* const> objects) {
	std::vector<std::vector<std::string>> holders(objects.size());
	std::vector<Target> targets;
	targets.reserve(objects.size());
	for (size_t i = 0; i < objects.size(); ++i)
		targets.push_back(Target{objects[i], dynamic_cast<Creature*>(objects[i]), &holders[i], {}});
	World::getInstance().forEachObject([&targets](VisibleObject& other) {
		try {
			findInObject(other, targets);
		} catch (const std::exception& e) {
			for (Target& target : targets)
				target.holders->push_back("? (" + other.toString() + " could not be searched: " + e.what() + ")");
		}
	});
	guarded("the world map instances", targets, [&targets] { findInMapInstances(targets); });
	for (Target& target : targets)
		guarded("the walker groups", std::span<Target>(&target, 1), [&target] { findInWalkerGroups(target); });
	guarded("the siege locations", targets, [&targets] { findInSiegeLocations(targets); });
	return holders;
}

std::vector<std::string> WorldLeakProbe::findHolders(VisibleObject& object) {
	VisibleObject* objects[] = {&object};
	return std::move(findHolders(objects)[0]);
}

void WorldLeakProbe::logHolders(std::span<const ProbedObject> objects) {
	if (objects.empty())
		return;
	std::vector<VisibleObject*> pointers;
	std::vector<std::string> names;
	for (const ProbedObject& probed : objects) {
		pointers.push_back(probed.object);
		names.push_back(std::string(probed.className != nullptr ? probed.className : "?") + " (object id " + std::to_string(probed.objectId) + ", " +
			probed.object->toString() + ")");
	}
	std::vector<std::vector<std::string>> holders = findHolders(pointers);
	for (size_t i = 0; i < pointers.size(); ++i) {
		if (holders[i].empty()) {
			log().warn("Leak probe: {}: no holder found. Searched by identity: {}. Not searched: {}", names[i], SEARCHED, NOT_SEARCHED);
			continue;
		}
		for (const std::string& holder : holders[i])
			log().warn("Leak probe: {} is held by {}", names[i], holder);
	}
}

void WorldLeakProbe::probe(std::span<const runtime::LeakCensus::ProbedLeak> leaks) {
	std::vector<ProbedObject> objects;
	for (const runtime::LeakCensus::ProbedLeak& leak : leaks) {
		if (auto* visibleObject = dynamic_cast<VisibleObject*>(leak.object))
			objects.push_back(ProbedObject{visibleObject, leak.className, leak.objectId});
	}
	logHolders(objects);
}

void WorldLeakProbe::install() noexcept {
	runtime::LeakCensus::getInstance().setHolderProbe(&WorldLeakProbe::probe);
}

} // namespace aion::gameserver::world
