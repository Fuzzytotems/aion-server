#pragma once

#include <concepts>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/world/fwd.h"
#include "aion/gameserver/world/knownlist/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * This class is representing visible objects. It's a base class for all in-game objects that can be spawned in the world at some particular position
 * (such as players, npcs).<br>
 * <br>
 * Objects of this class, as can be spawned in game, can be seen by other visible objects. To keep track of which objects are already "known" by this
 * visible object and which are not, VisibleObject is containing {@link KnownList} which is responsible for holding this information.
 * <p>
 * Hub header (docs/design/hub-headers.md, reference implementation). Construction is two-phase (handlers-and-porting-plan.md §1.7): Java
 * `new Npc(...)` is `VisibleObject::create<Npc>(...)`, which constructs through makeRef with the CreateKey passkey and then runs the virtual
 * postConstruct() chain (the Java constructor-body work that needs the dynamic type: AI creation, `controller.setOwner(this)`, stat
 * containers). Subclasses never declare their own `create`; their constructors take `CreateKey` first and only store members.
 * C++ only: runtime::ZombieBreakable (breakKnownEdges, the zombie breaker of runtime-architecture.md §5.3) and breakTarget() for the logout and
 * delete breakers (model/gameobjects/player/LogoutBreakers.h).
 *
 * @author -Nemesiss-
 */
class VisibleObject : public AionObject, public runtime::ZombieBreakable {
	AION_MAKE_REF_FRIEND
protected:
	/** Passkey of every VisibleObject constructor: only create<T> can make one, so postConstruct() can never be skipped. */
	struct CreateKey {
	private:
		CreateKey() = default;
		friend class VisibleObject;
	};

private:
	const templates::VisibleObjectTemplate* objectTemplate;

protected:
	/** Position of object in the world. */
	runtime::Field<runtime::Ref<world::WorldPosition>> position;

private:
	/** KnownList of this VisibleObject. */
	runtime::PartSlot<world::knownlist::KnownList> knownlist{*this};
	/** Controller of this VisibleObject (late-bound part: subclasses call setOwner in postConstruct) */
	const std::unique_ptr<controllers::VisibleObjectController> controller;
	/** Visible object's target: the object itself is held without a reference (RT-4 TargetField; cycles.toml cpp-breaker) */
	runtime::SelfOrRef<VisibleObject> target{*this};
	/** Spawn template of this visibleObject. */
	const runtime::Ref<templates::spawns::SpawnTemplate> spawnTemplate;

public:
	/**
	 * Java `new T(args...)` for every VisibleObject class: constructs T (count 1) and runs postConstruct() before anyone else can see it.
	 * @throws whatever T's constructor or postConstruct() throws (the object is then released)
	 */
	template <std::derived_from<VisibleObject> T, class... Args>
	[[nodiscard]] static runtime::Ref<T> create(Args&&... args) {
		runtime::Ref<T> object = runtime::makeRef<T>(CreateKey(), std::forward<Args>(args)...);
		static_cast<VisibleObject&>(*object).postConstruct();
		return object;
	}

protected:
	/** Constructor. */
	VisibleObject(CreateKey key, int32_t objId, std::unique_ptr<controllers::VisibleObjectController> controller,
		runtime::Ptr<templates::spawns::SpawnTemplate> spawnTemplate, const templates::VisibleObjectTemplate* objectTemplate,
		runtime::Ptr<world::WorldPosition> position, bool autoReleaseObjectId);
	~VisibleObject() override;

	/**
	 * C++ only: the part of the Java constructor bodies that needs the dynamic type, run by create<T> right after construction. Overrides call
	 * the base class version first, then the statements of their Java constructor body in Java order. VisibleObject's own constructor body has
	 * nothing to add.
	 */
	virtual void postConstruct() {}

public:
	std::string getName() override;

	int32_t getInstanceId();

	/** Return World map id. */
	int32_t getWorldId();

	/** Return the WorldType of the current location */
	virtual world::WorldType getWorldType();

	world::WorldDropType getWorldDropType();

	/** Return World position x */
	virtual float getX();

	/** Return World position y */
	virtual float getY();

	/** Return World position z */
	virtual float getZ();

	/** Heading of the object. Values from <0,120) */
	virtual int8_t getHeading();

	/** Return object position */
	runtime::Ptr<world::WorldPosition> getPosition() const { return position.get(); }

	runtime::Ptr<world::WorldMapInstance> getWorldMapInstance();

	/** Check if object is spawned. */
	bool isSpawned();

	/** @return True if the object is in the world (can be a spawned or despawned object) */
	bool isInWorld();

	/** Check if map is instance */
	bool isInInstance();

	void clearKnownlist();

	void clearKnownlist(animations::ObjectDeleteAnimation animation);

	void updateKnownlist();

	/** @return True, if the object is able to see the given object (basic check without distance validation) */
	virtual bool canSee(runtime::Ptr<VisibleObject> object);

	/** Set KnownList to this VisibleObject (replaces the part; the previous one stays with its owner, PartSlot RetireTo::OWNER) */
	void setKnownlist(std::unique_ptr<world::knownlist::KnownList> knownlist);

	/** Returns KnownList of this VisibleObject. @throws NullPointerException before setKnownlist (Java: null) */
	world::knownlist::KnownList& getKnownList() const;

	/** Return VisibleObjectController of this VisibleObject. Subclasses narrow it (Npc: NpcController&). */
	controllers::VisibleObjectController& getController() const;

	runtime::Ptr<VisibleObject> getTarget() const { return target.get(); }

	void setTarget(runtime::Ptr<VisibleObject> creature);

	/**
	 * C++ only (LogoutBreakers L1/D1, cycles.toml `VisibleObject.target`): clears the target without onTargetChanged or any other notification.
	 * Idempotent.
	 */
	void breakTarget() noexcept;

	/** @return target is object with id equal to objectId */
	bool isTargeting(int32_t objectId);

	/** Return spawnTemplate template of this VisibleObject. SiegeNpc narrows it. */
	runtime::Ptr<templates::spawns::SpawnTemplate> getSpawn() const { return spawnTemplate; }

	/**
	 * @return the objectTemplate. Subclasses narrow it (Npc: const NpcTemplate*). Immortal static data, except for a Player: its template is its
	 *         RefCounted PlayerCommonData, which the `const VisibleObjectTemplate*` type cannot show, so tasks never capture this pointer for a
	 *         Player (capture the Player or Ref<PlayerCommonData>).
	 */
	const templates::VisibleObjectTemplate* getObjectTemplate() const { return objectTemplate; }

	virtual void setPosition(runtime::Ptr<world::WorldPosition> position);

	/** @return Distance in in-game meters how far this object can be seen/detected (a.k.a. known) */
	virtual float getVisibleDistance() { return 95; }

	std::string toString() override;

	/** C++ only (runtime::ZombieBreakable): cuts the zombie-safe edges of this object, LogoutBreakers::breakZombieEdges(*this). */
	std::vector<const char*> breakKnownEdges() override;
};

} // namespace aion::gameserver::model::gameobjects
