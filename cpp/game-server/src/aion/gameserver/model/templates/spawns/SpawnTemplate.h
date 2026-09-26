#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/templates/event/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/spawnengine/fwd.h"

namespace aion::gameserver::model::templates::spawns {

/**
 * Hub header (docs/design/hub-headers.md). An OwnedPart of its SpawnGroup (runtime-architecture.md §9 "Spawn family", §2.3 PartList): the group
 * owns its templates in `spots` (or `detachedTemplates`), `spawnGroup` is the non-retaining OwnerRef, and a `Ref<SpawnTemplate>`
 * (VisibleObject.spawnTemplate, SpawnEngine factories) retains the group. So a single-time spawn's group and template live exactly as long as
 * something references the template.
 * Construction: Java `new SpawnTemplate(group, x, y, z, ...)` (the 8- and 10-argument constructors end with addTemplate(), i.e.
 * `spawnGroup.addSpawnTemplate(this)`) is `SpawnTemplate::create(group, ...)`: the part is constructed and then moved into the group by the
 * static addTemplate(std::unique_ptr), because a constructor cannot hand over ownership of itself. A subclass using those constructors
 * (BaseSpawnTemplate, SiegeSpawnTemplate, RiftSpawnTemplate, VortexSpawnTemplate, AhserionsFlightSpawnTemplate, TownSpawnTemplate) does the same
 * in its create(); a template that is constructed but never added dies with its unique_ptr, so a missing add fails at once. The spot constructor
 * does not add itself in Java either: SpawnGroup's constructors add those templates to `spots`, Town.java:138 uses
 * SpawnGroup::adoptDetachedTemplate.
 * The walker id and the AI name distinguish null from a value in Java (Npc.isPathWalker, Creature's AI selection, `setWalkerId(null)` in
 * handlers): `std::optional` in signatures, the empty string in the fields (Field<std::string>).
 *
 * @author xTz, Rolandas
 */
class SpawnTemplate : public runtime::OwnedPart {
public:
	static constexpr std::string_view NO_AI = "__NO_AI__";

private:
	runtime::Field<float> x{};
	runtime::Field<float> y{};
	runtime::Field<float> z{};
	runtime::Field<int8_t> h{};
	runtime::Field<int32_t> staticId{};
	const int32_t randomWalk;
	runtime::Field<std::string> walkerId{};
	const std::optional<int32_t> walkerIdx;
	const std::string anchor;
	runtime::OwnerRef<SpawnGroup> spawnGroup;
	const std::string aiName;
	const int32_t state;
	const bool aerialSpawn;
	const int32_t creatorId;
	const TemporarySpawn* temporarySpawn;

public:
	/** Java `new SpawnTemplate(spawnGroup, spot)`: a part the caller hands to its group (SpawnGroup constructors, adoptDetachedTemplate) */
	SpawnTemplate(SpawnGroup& spawnGroup, const SpawnSpotTemplate* spot);

	/** Does not add itself to the group (Java: addTemplate() in the delegated constructor; C++: create does, see the class comment) */
	SpawnTemplate(SpawnGroup& spawnGroup, float x, float y, float z, int8_t heading, int32_t randWalk, std::optional<std::string_view> walkerId,
		int32_t staticId);

	/** Does not add itself to the group (Java: addTemplate(); C++: create does, see the class comment) */
	SpawnTemplate(SpawnGroup& spawnGroup, float x, float y, float z, int8_t heading, int32_t randWalk, std::optional<std::string_view> walkerId,
		int32_t staticId, int32_t creatorId, std::optional<std::string_view> aiName);

	~SpawnTemplate() override;

	/** Java `new SpawnTemplate(spawnGroup, x, y, z, heading, randWalk, walkerId, staticId)`: constructs, then addTemplate() */
	static runtime::Ref<SpawnTemplate> create(SpawnGroup& spawnGroup, float x, float y, float z, int8_t heading, int32_t randWalk,
		std::optional<std::string_view> walkerId, int32_t staticId);

	/** Java `new SpawnTemplate(spawnGroup, x, y, z, heading, randWalk, walkerId, staticId, creatorId, aiName)`: constructs, then addTemplate() */
	static runtime::Ref<SpawnTemplate> create(SpawnGroup& spawnGroup, float x, float y, float z, int8_t heading, int32_t randWalk,
		std::optional<std::string_view> walkerId, int32_t staticId, int32_t creatorId, std::optional<std::string_view> aiName);

protected:
	/**
	 * Java private `addTemplate()`: `spawnGroup.addSpawnTemplate(this)`. C++: moves the constructed part into its own group and returns the
	 * stored template; protected for the create() of subclasses.
	 */
	static SpawnTemplate& addTemplate(std::unique_ptr<SpawnTemplate> spawnTemplate);

public:
	float getX() const { return x.get(); }

	void setX(float value) { x.set(value); }

	float getY() const { return y.get(); }

	void setY(float value) { y.set(value); }

	float getZ() const { return z.get(); }

	void setZ(float value) { z.set(value); }

	int8_t getHeading() const { return h.get(); }

	void setHeading(int8_t value) { h.set(value); }

	int32_t getStaticId() const { return staticId.get(); }

	void setStaticId(int32_t value) { staticId.set(value); }

	int32_t getRandomWalkRange() const { return randomWalk; }

	int32_t getNpcId();

	int32_t getWorldId();

	runtime::Ptr<SpawnTemplate> changeTemplate(int32_t instanceId);

	int32_t getRespawnTime();

	void resetPoolSpot(int32_t instanceId);

	const TemporarySpawn* getTemporarySpawn();

	/** @return the handler type of the group, std::nullopt for Java null */
	std::optional<spawnengine::SpawnHandlerType> getHandlerType();

	std::string getAnchor() const { return anchor; }

	bool isNoRespawn();

	bool hasPool();

	/** @return the walker id, std::nullopt for Java null */
	std::optional<std::string> getWalkerId();

	void setWalkerId(std::optional<std::string_view> walkerId);

	std::optional<int32_t> getWalkerIndex() const { return walkerIdx; }

	/** The owning group (Java never null) */
	SpawnGroup& getGroup() const { return spawnGroup; }

	bool isTemporarySpawn();

	bool isEventSpawn();

	const event::EventTemplate* getEventTemplate();

	/** @return the AI name, std::nullopt for Java null */
	std::optional<std::string> getAiName();

	int32_t getState() const { return state; }

	bool isAerialSpawn() const { return aerialSpawn; }

	int32_t getCreatorId() const { return creatorId; }
};

} // namespace aion::gameserver::model::templates::spawns
