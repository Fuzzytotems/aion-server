#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/model/templates/spawns/riftspawns/fwd.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/fwd.h"
#include "aion/gameserver/model/templates/spawns/vortexspawns/fwd.h"
#include "aion/gameserver/skillengine/effect/fwd.h"
#include "aion/gameserver/spawnengine/fwd.h"

namespace aion::gameserver::spawnengine {

/**
 * Creates the visible objects of spawn templates and brings them into the world.
 * <p>
 * C++: a static-only class. The factories return the new object as a Ref (hub-headers.md §5: a newly created object); null where Java returns
 * null. Java's protected methods (called by SpawnEngine in the same package) are public.
 *
 * @author ATracer
 */
class VisibleObjectSpawner {
public:
	VisibleObjectSpawner() = delete;

	/** Java protected */
	static runtime::Ref<model::gameobjects::VisibleObject> spawnNpc(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex);

	static runtime::Ref<model::gameobjects::SummonedHouseNpc> spawnHouseNpc(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex,
		model::house::House& creator);

	/** Java protected */
	static runtime::Ref<model::gameobjects::VisibleObject> spawnRiftNpc(model::templates::spawns::riftspawns::RiftSpawnTemplate& spawn,
		int32_t instanceIndex);

	/** Java protected */
	static runtime::Ref<model::gameobjects::VisibleObject> spawnSiegeNpc(model::templates::spawns::siegespawns::SiegeSpawnTemplate& spawn,
		int32_t instanceIndex);

	/** Java protected */
	static runtime::Ref<model::gameobjects::VisibleObject> spawnInvasionNpc(model::templates::spawns::vortexspawns::VortexSpawnTemplate& spawn,
		int32_t instanceIndex);

	static runtime::Ref<model::gameobjects::Gatherable> spawnGatherable(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex);

	static runtime::Ref<model::gameobjects::Trap> spawnTrap(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex,
		model::gameobjects::Creature& creator);

	static runtime::Ref<model::gameobjects::GroupGate> spawnGroupGate(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex,
		model::gameobjects::Creature& creator);

	static runtime::Ref<model::gameobjects::Kisk> spawnKisk(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex,
		model::gameobjects::player::Player& creator);

	/** Spawns postman for express mail (@author ViAl) */
	static runtime::Ref<model::gameobjects::Npc> spawnPostman(model::gameobjects::player::Player& owner);

	static runtime::Ref<model::gameobjects::Npc> spawnFunctionalNpc(model::gameobjects::player::Player& owner, int32_t npcId,
		skillengine::effect::SummonOwner summonOwner);

	static runtime::Ref<model::gameobjects::Servant> spawnServant(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex,
		model::gameobjects::Creature& creator, int32_t level, model::gameobjects::NpcObjectType objectType);

	static runtime::Ref<model::gameobjects::Servant> spawnEnemyServant(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex,
		model::gameobjects::Creature& creator, int8_t servantLvl);

	static runtime::Ref<model::gameobjects::Homing> spawnHoming(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex,
		model::gameobjects::Creature& creator, int32_t attackCount, int32_t skillId);

	static runtime::Ref<model::gameobjects::Summon> spawnSummon(model::gameobjects::player::Player& creator, int32_t npcId, int32_t skillId,
		int32_t time);

	/** @return the pet, null if the player has no such pet or there is no template */
	static runtime::Ref<model::gameobjects::Pet> spawnPet(model::gameobjects::player::Player& player, int32_t templateId);
};

} // namespace aion::gameserver::spawnengine
