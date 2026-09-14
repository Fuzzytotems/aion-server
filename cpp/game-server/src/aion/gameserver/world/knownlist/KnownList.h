#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/world/knownlist/fwd.h"

namespace aion::gameserver::world::knownlist {

/**
 * The KnownList contains every object the owner currently knows (visible and invisible). Which objects are found is controlled by distance and
 * awareness modifiers.<br>
 * Knowing is always a two-way relation, so if A knows B, then B also knows A. If just one is not aware of the other, both can't know each other.
 * <p>
 * Hub header (docs/design/hub-headers.md). A part of its VisibleObject (fieldmap base OwnedPart, `VisibleObject::knownlist` is a PartSlot):
 * created with `std::make_unique<X>(owner)` and handed to `VisibleObject::setKnownlist`. C++ additions (runtime-architecture.md §5.3 RR-14):
 * every pair add goes through the static addPair handshake (lint L17 rejects other `add(` calls), and del is protected instead of private,
 * because FlagKnownList's one-sided removeIf is ported as a two-sided del (DEVIATION).
 *
 * @author -Nemesiss-, kosyachok, Neon
 */
class KnownList : public runtime::OwnedPart {
protected:
	runtime::OwnerRef<model::gameobjects::VisibleObject> owner;
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<KnownObject>> knownObjects{};

public:
	explicit KnownList(model::gameobjects::VisibleObject& owner);
	~KnownList() override;

	/**
	 * Updates the cached visibility state of all objects in this list and adds or removes objects based on their current distance to the owner.
	 */
	virtual void update(); // synchronized

	/** Removes all objects from this list and sends a despawn animation for the owner to all removed players. */
	void clear(model::animations::ObjectDeleteAnimation animation); // synchronized

	/** Checks if owner knows the object. */
	bool knows(model::gameobjects::VisibleObject& object);

	/** Checks if owner sees the object. */
	bool sees(model::gameobjects::VisibleObject& object);

protected:
	/**
	 * C++ only (runtime-architecture.md §5.3 RR-14): the pair handshake of KnownList.java:175-178, :192-193 and FlagKnownList.java:18-21.
	 * `b.getKnownList().add(a) && a.getKnownList().add(b)`, then, if either object is no longer spawned (World.despawn raced), both edges are
	 * deleted again with ObjectDeleteAnimation::NONE and false is returned.
	 *
	 * @return true if both objects know each other now
	 */
	static bool addPair(model::gameobjects::VisibleObject& a, model::gameobjects::VisibleObject& b);

	/** Only called by addPair (lint L17). */
	bool add(model::gameobjects::VisibleObject& object);

public:
	/** Updates the object's cached visibility state in this list, depending on the current see state of the owner. */
	void updateVisibleObject(model::gameobjects::VisibleObject& object);

private:
	void updateVisibility(KnownObject& knownObject);

	void updatePetVisibility(KnownObject& knownObject);

protected:
	/**
	 * Removes VisibleObject from this KnownList and deletes it. Java private; protected in C++ for addPair's rollback and FlagKnownList.
	 *
	 * @param animation
	 *          - the disappear animation others will see
	 */
	void del(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation);

private:
	void notifySee(model::gameobjects::VisibleObject& object);

	void notifyNotSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation);

	void notifyNotKnow(model::gameobjects::VisibleObject& object);

	/** forget out of distance objects or update visible state. */
	void forgetObjectsOrUpdateVisibility();

protected:
	/** Find objects that are in visibility range. */
	void findVisibleObjects();

	/** @return True if the knownlist owner is aware of newObject (should be kept in knownlist) */
	virtual bool isAwareOf(runtime::Ptr<model::gameobjects::VisibleObject> newObject);

	/** @return Detection radius in meters of this knownlist. */
	virtual float getVisibleDistance();

private:
	bool isInRange(model::gameobjects::VisibleObject& newObject);

public:
	runtime::Ptr<model::gameobjects::VisibleObject> findObject(const std::function<bool(KnownObject&)>& predicate);

	runtime::Ptr<model::gameobjects::VisibleObject> getObject(int32_t targetObjectId);

	runtime::Ptr<model::gameobjects::player::Player> getPlayer(int32_t targetObjectId);

	void forEach(const std::function<void(KnownObject&)>& action);

	void forEachObject(const std::function<void(model::gameobjects::VisibleObject&)>& consumer);

	void forEachNpc(const std::function<void(model::gameobjects::Npc&)>& consumer);

	void forEachPlayer(const std::function<void(model::gameobjects::player::Player&)>& consumer);

	/** Java: Stream<KnownObject> (hub-headers.md §7.2: a snapshot under the Java name) */
	std::vector<runtime::Ptr<KnownObject>> stream();

	/** Java: Stream<Player> */
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> streamPlayers();

	/** Java: Stream<Player> */
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> streamVisiblePlayers();
};

} // namespace aion::gameserver::world::knownlist
