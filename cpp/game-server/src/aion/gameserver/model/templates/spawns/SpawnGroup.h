#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/base/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/model/templates/event/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/model/vortex/fwd.h"
#include "aion/gameserver/services/panesterra/ahserion/fwd.h"
#include "aion/gameserver/spawnengine/fwd.h"

namespace aion::gameserver::model::templates::spawns {

/**
 * Hub header (docs/design/hub-headers.md). RefCounted (fieldmap K4: SpawnsData.allSpawnMaps). `SpawnGroup::create(...)` replaces the public Java
 * constructors. The spawn templates are parts of their group (runtime-architecture.md §9 "Spawn family"): `spots` is an append-only
 * PartList (stable addresses, lock-free readers), so a Ref to a template retains the group and the group -> template -> group cycle of Java
 * does not exist. The constructors that take a Spawn add one `std::make_unique<XSpawnTemplate>(*this, spot)` per spot, like Java.
 * `poolUsedTemplates` holds sibling pointers to the group's own templates. C++ only: `detachedTemplates` owns templates that Java creates for
 * a group without adding them to its spots (Town.java:138), so the pool and respawn logic never sees them. The pool map always exists (Java:
 * Collections.emptyMap() without a pool).
 *
 * @author xTz, Rolandas
 */
class SpawnGroup : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t worldId;
	const int32_t npcId;
	const int32_t pool;
	const int32_t respawnTime;
	const int8_t difficultId;
	const std::optional<spawnengine::SpawnHandlerType> handlerType;
	const TemporarySpawn* temporarySpawn;
	runtime::PartList<SpawnTemplate> spots{*this};
	// fieldmap: sibling pointers to the group's own template parts (build/s0b-cycles-work/setII.toml, cycles.toml `part`)
	runtime::HashMap<int32_t, runtime::Ref<runtime::RcHashSet<SpawnTemplate*>>> poolUsedTemplates{AION_LOCK_CLASS(SpawnGroup::poolUsedTemplates)};
	const event::EventTemplate* eventTemplate;
	/** C++ only: templates of this group that are not spawn spots (adoptDetachedTemplate) */
	runtime::PartList<SpawnTemplate> detachedTemplates{*this};

protected:
	SpawnGroup(int32_t worldId, int32_t npcId, int32_t respawnTime, const event::EventTemplate* eventTemplate);

	/** Adds a SpawnTemplate per spot */
	SpawnGroup(int32_t worldId, const Spawn* spawn);

	/** Adds a BaseSpawnTemplate (id, occupier) per spot */
	SpawnGroup(int32_t worldId, const Spawn* spawn, int32_t id, model::base::BaseOccupier occupier);

	/** Adds a RiftSpawnTemplate (id) per spot */
	SpawnGroup(int32_t worldId, const Spawn* spawn, int32_t id);

	/** Adds a VortexSpawnTemplate (id, state type) per spot */
	SpawnGroup(int32_t worldId, const Spawn* spawn, int32_t id, model::vortex::VortexStateType type);

	/** Adds a SiegeSpawnTemplate per spot */
	SpawnGroup(int32_t worldId, const Spawn* spawn, int32_t siegeId, model::siege::SiegeRace race, model::siege::SiegeModType mod);

	/** For Ahserion's Flight. Adds an AhserionsFlightSpawnTemplate (stage, faction) per spot */
	SpawnGroup(int32_t worldId, const Spawn* spawn, int32_t stage, services::panesterra::ahserion::PanesterraFaction faction);

private:
	/** @param spots always empty in Java (`new ArrayList<>(capacity)`); parts must be bound to this group, so the callers pass none */
	SpawnGroup(int32_t worldId, const Spawn* spawn, std::vector<std::unique_ptr<SpawnTemplate>> spots);

	/** @param spots parts of this group to add to the spot list (Java keeps the given list; always empty) */
	SpawnGroup(int32_t worldId, int32_t npcId, int32_t pool, int32_t respawnTime, int8_t difficultId,
		std::optional<spawnengine::SpawnHandlerType> handlerType, const TemporarySpawn* temporarySpawn,
		std::vector<std::unique_ptr<SpawnTemplate>> spots, const event::EventTemplate* eventTemplate);

protected:
	~SpawnGroup() override;

public:
	static runtime::Ref<SpawnGroup> create(int32_t worldId, int32_t npcId, int32_t respawnTime, const event::EventTemplate* eventTemplate);

	static runtime::Ref<SpawnGroup> create(int32_t worldId, const Spawn* spawn);

	static runtime::Ref<SpawnGroup> create(int32_t worldId, const Spawn* spawn, int32_t id, model::base::BaseOccupier occupier);

	static runtime::Ref<SpawnGroup> create(int32_t worldId, const Spawn* spawn, int32_t id);

	static runtime::Ref<SpawnGroup> create(int32_t worldId, const Spawn* spawn, int32_t id, model::vortex::VortexStateType type);

	static runtime::Ref<SpawnGroup> create(int32_t worldId, const Spawn* spawn, int32_t siegeId, model::siege::SiegeRace race,
		model::siege::SiegeModType mod);

	/** For Ahserion's Flight */
	static runtime::Ref<SpawnGroup> create(int32_t worldId, const Spawn* spawn, int32_t stage,
		services::panesterra::ahserion::PanesterraFaction faction);

	/** @return the live spot list (append-only; iterate `getSpawnTemplates().snapshot()`) */
	runtime::PartList<SpawnTemplate>& getSpawnTemplates() { return spots; }

	/**
	 * Java: `synchronized (spots) { spots.add(spawnTemplate); }` (PartList::add runs under the list's own Monitor).
	 * @param spawnTemplate a template of this group (checked builds: C11)
	 * @return the stored template
	 */
	SpawnTemplate& addSpawnTemplate(std::unique_ptr<SpawnTemplate> spawnTemplate);

	/**
	 * C++ only: stores a template of this group that Java creates without adding it to the spots (Town.java:138 `new TownSpawnTemplate(group,
	 * spot, id)`), so a Ref to it retains the group like every other template.
	 * @return the stored template
	 */
	SpawnTemplate& adoptDetachedTemplate(std::unique_ptr<SpawnTemplate> spawnTemplate);

	int32_t getWorldId() const { return worldId; }

	int32_t getNpcId() const { return npcId; }

	const TemporarySpawn* getTemporarySpawn() const { return temporarySpawn; }

	int32_t getPool() const { return pool; }

	bool hasPool();

	int8_t getDifficultId() const { return difficultId; }

	int32_t getRespawnTime() const { return respawnTime; }

	bool isTemporarySpawn();

	/** @return the handler type, std::nullopt for Java null */
	std::optional<spawnengine::SpawnHandlerType> getHandlerType() const { return handlerType; }

	runtime::Ptr<SpawnTemplate> reserveRandomFreePoolSpot(int32_t instanceId);

	void resetPoolSpot(int32_t instanceId, SpawnTemplate& template_);

	/** Call it before each randomization to unset all template use. */
	void resetPoolSpots(int32_t instanceId);

	const event::EventTemplate* getEventTemplate() const { return eventTemplate; }
};

} // namespace aion::gameserver::model::templates::spawns
