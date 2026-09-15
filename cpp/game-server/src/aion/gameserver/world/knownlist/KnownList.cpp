#include "aion/gameserver/world/knownlist/KnownList.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/collections/CollectionUtil.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::world::knownlist {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.world.knownlist.KnownList");

using model::animations::ObjectDeleteAnimation;
using model::gameobjects::VisibleObject;

namespace {

constexpr size_t PAIR_LOCK_STRIPES = 64;

/**
 * C++ only: the striped pair locks. Every membership change of a pair (KnownList::addPair, removePair, delPair) holds the lock of the pair's
 * stripe, and nothing else is locked or called under it except the two knownObjects maps (lock order: KnownList monitors, pair lock,
 * knownObjects stripes). Immortal and thread-safe.
 */
constinit runtime::Monitor pairLocks[PAIR_LOCK_STRIPES];

runtime::MonitorHandle pairLock(const VisibleObject& a, const VisibleObject& b) {
	auto low = static_cast<uint32_t>(std::min(a.getObjectId(), b.getObjectId()));
	auto high = static_cast<uint32_t>(std::max(a.getObjectId(), b.getObjectId()));
	uint64_t hash = ((uint64_t{low} << 32) | high) * 0x9E3779B97F4A7C15ull;
	return {pairLocks[hash >> 58], AION_LOCK_CLASS(KnownList::pairLocks#stripe)};
}

} // namespace

KnownList::KnownList(VisibleObject& ownerValue) : OwnedPart(ownerValue), owner(ownerValue) {
}

KnownList::~KnownList() = default;

void KnownList::update() {
	SYNCHRONIZED(*this) {
		forgetObjectsOrUpdateVisibility();
		findVisibleObjects();
	}
}

void KnownList::clear(ObjectDeleteAnimation animation) {
	SYNCHRONIZED(*this) {
		for (runtime::Ptr<KnownObject> object : knownObjects.values()) {
			// Java: del(object.get(), NONE); object.get().getKnownList().del(owner, animation);
			removePair(owner, *object->get(), ObjectDeleteAnimation::NONE, animation);
		}
	}
}

bool KnownList::clearWithoutNotify() {
	// C++ only (zombie breaker): drop every known object without notSee/notKnow, packets or the other side's list
	bool hadObjects = !knownObjects.isEmpty();
	knownObjects.clear();
	return hadObjects;
}

bool KnownList::knows(VisibleObject& object) {
	return knownObjects.containsKey(object.getObjectId());
}

bool KnownList::sees(VisibleObject& object) {
	runtime::Ptr<KnownObject> knownObject = knownObjects.get(object.getObjectId());
	return knownObject && knownObject->isVisible();
}

bool KnownList::addPair(VisibleObject& a, VisibleObject& b) {
	// Java: `if (b.getKnownList().add(a)) a.getKnownList().add(b);` at the three pair-add sites (KnownList.java:175-178, :192-193,
	// FlagKnownList.java:18-21). C++ (runtime-architecture.md §5.3, RR-17): both inserts and the Dekker handshake with World.despawn (which sets
	// isSpawned(false) before it clears the known list) run under the pair's striped lock, which every two-sided removal (clear,
	// forgetObjectsOrUpdateVisibility, FlagKnownList.update) takes as well. So a pair is always present on both sides or on neither, and a
	// concurrent rollback can never delete an edge another addPair has inserted. The see notifications follow after the lock is released, in
	// Java's order (b sees a, then a sees b); a rolled back insert sends no see notification (DEVIATION, P4-10.md).
	runtime::Ptr<KnownObject> knownByB;
	runtime::Ptr<KnownObject> knownByA;
	bool rolledBack = false;
	{
		runtime::MonitorGuard guard(pairLock(a, b));
		knownByB = b.getKnownList().insert(a);
		if (!knownByB)
			return false;
		knownByA = a.getKnownList().insert(b); // fails only if a is not aware of b: a pair is never one-sided under the lock
		if (!knownByA || !a.isSpawned() || !b.isSpawned()) {
			b.getKnownList().knownObjects.remove(a.getObjectId());
			if (knownByA)
				a.getKnownList().knownObjects.remove(b.getObjectId());
			rolledBack = true;
		}
	}
	if (rolledBack) {
		// the removal notifications of del (notSee only if a lock-free visibility update of another thread sent see in the meantime)
		b.getKnownList().notifyRemoved(*knownByB, ObjectDeleteAnimation::NONE);
		if (knownByA)
			a.getKnownList().notifyRemoved(*knownByA, ObjectDeleteAnimation::NONE);
		return false;
	}
	b.getKnownList().updateVisibility(*knownByB);
	a.getKnownList().updateVisibility(*knownByA);
	return true;
}

void KnownList::delPair(VisibleObject& a, VisibleObject& b) {
	// FlagKnownList.update: Java's removeIf removes the entries without any notification; C++ removes both edges (RR-17), also silently
	runtime::MonitorGuard guard(pairLock(a, b));
	a.getKnownList().knownObjects.remove(b.getObjectId());
	b.getKnownList().knownObjects.remove(a.getObjectId());
}

void KnownList::removePair(VisibleObject& a, VisibleObject& b, ObjectDeleteAnimation aAnimation, ObjectDeleteAnimation bAnimation) {
	// Java: `a.del(b, aAnimation); b.getKnownList().del(a, bAnimation);` - both removals under the pair lock, then the notifications in that order
	runtime::Ptr<KnownObject> knownByA;
	runtime::Ptr<KnownObject> knownByB;
	{
		runtime::MonitorGuard guard(pairLock(a, b));
		knownByA = a.getKnownList().knownObjects.remove(b.getObjectId());
		knownByB = b.getKnownList().knownObjects.remove(a.getObjectId());
	}
	if (knownByA)
		a.getKnownList().notifyRemoved(*knownByA, aAnimation);
	if (knownByB)
		b.getKnownList().notifyRemoved(*knownByB, bAnimation);
}

runtime::Ptr<KnownObject> KnownList::insert(VisibleObject& object) {
	if (!isAwareOf(runtime::Ptr<VisibleObject>(object)))
		return nullptr;
	runtime::Ref<KnownObject> knownObject = KnownObject::create(object);
	if (knownObjects.putIfAbsent(object.getObjectId(), knownObject))
		return nullptr;
	return knownObject;
}

void KnownList::notifyRemoved(KnownObject& knownObject, ObjectDeleteAnimation animation) {
	if (knownObject.updateVisible(false))
		notifyNotSee(*knownObject.get(), animation);
	notifyNotKnow(*knownObject.get());
}

bool KnownList::add(VisibleObject& object) {
	// Java body; the C++ pair sites use addPair, which inserts and notifies in separate steps
	runtime::Ptr<KnownObject> knownObject = insert(object);
	if (!knownObject)
		return false;
	updateVisibility(*knownObject);
	return true;
}

void KnownList::updateVisibleObject(VisibleObject& object) {
	runtime::Ptr<KnownObject> knownObject = knownObjects.get(object.getObjectId());
	if (knownObject)
		updateVisibility(*knownObject);
}

void KnownList::updateVisibility(KnownObject& knownObject) {
	bool visible = owner.canSee(knownObject.get());
	if (!knownObject.updateVisible(visible))
		return;
	if (visible) {
		notifySee(*knownObject.get());
	} else {
		notifyNotSee(*knownObject.get(), ObjectDeleteAnimation::FADE_OUT);
	}
	updatePetVisibility(knownObject); // pet spawn packet must be sent after SM_PLAYER_INFO, otherwise the pet will not be displayed
}

void KnownList::updatePetVisibility(KnownObject& knownObject) {
	if (auto player = runtime::as<model::gameobjects::player::Player>(knownObject.get())) {
		if (runtime::Ptr<model::gameobjects::Pet> pet = player->getPet()) {
			runtime::Ptr<KnownObject> petKnownObject = knownObjects.get(pet->getObjectId());
			if (petKnownObject)
				updateVisibility(*petKnownObject);
		}
	}
}

void KnownList::del(VisibleObject& object, ObjectDeleteAnimation animation) {
	// Java body; the C++ removals of a pair use removePair, which removes both edges under the pair lock and then notifies
	runtime::Ptr<KnownObject> knownObject = knownObjects.remove(object.getObjectId());
	if (knownObject)
		notifyRemoved(*knownObject, animation);
}

void KnownList::notifySee(VisibleObject& object) {
	try {
		owner.getController().see(object);
	} catch (const std::exception& e) {
		log.error("", e);
	}
}

void KnownList::notifyNotSee(VisibleObject& object, ObjectDeleteAnimation animation) {
	try {
		owner.getController().notSee(object, animation);
	} catch (const std::exception& e) {
		log.error("", e);
	}
}

void KnownList::notifyNotKnow(VisibleObject& object) {
	try {
		owner.getController().notKnow(object);
	} catch (const std::exception& e) {
		log.error("", e);
	}
}

void KnownList::forgetObjectsOrUpdateVisibility() {
	for (runtime::Ptr<KnownObject> object : knownObjects.values()) {
		if (isInRange(*object->get())) {
			updateVisibility(*object);
		} else {
			// Java: del(object.get(), NONE); object.get().getKnownList().del(owner, NONE);
			removePair(owner, *object->get(), ObjectDeleteAnimation::NONE, ObjectDeleteAnimation::NONE);
		}
	}
}

void KnownList::findVisibleObjects() {
	if (!owner.isSpawned())
		return;

	runtime::Ptr<WorldPosition> position = owner.getPosition();
	if (dynamic_cast<model::gameobjects::player::Player*>(&owner) != nullptr) {
		position->getWorldMapInstance()->forEachNpc([this](model::gameobjects::Npc& npc) {
			if (npc.isFlag())
				addPair(owner, npc); // Java: if (npc.getKnownList().add(owner)) add(npc);
		});
	}
	for (MapRegion* region : *position->getMapRegion()->getNeighbours()) {
		for (runtime::Ptr<VisibleObject> newObject : region->getObjects().values()) {
			if (!isAwareOf(newObject))
				continue;

			if (knows(*newObject))
				continue;

			if (!isInRange(*newObject))
				continue;

			addPair(owner, *newObject); // Java: if (newObject.getKnownList().add(owner)) add(newObject);
		}
	}
}

bool KnownList::isAwareOf(runtime::Ptr<VisibleObject> newObject) {
	return newObject && !newObject->equals(owner);
}

float KnownList::getVisibleDistance() {
	return owner.getVisibleDistance();
}

bool KnownList::isInRange(VisibleObject& newObject) {
	// the two-way relation of KnownLists requires checking the maximum valid distance for both objects to avoid flickering and other display errors
	float distance = std::max(getVisibleDistance(), newObject.getKnownList().getVisibleDistance());
	return utils::PositionUtil::isInRange(owner, newObject, distance);
}

runtime::Ptr<VisibleObject> KnownList::findObject(const std::function<bool(KnownObject&)>& predicate) {
	for (runtime::Ptr<KnownObject> value : knownObjects.values()) {
		if (predicate(*value))
			return value->get();
	}
	return nullptr;
}

runtime::Ptr<VisibleObject> KnownList::getObject(int32_t targetObjectId) {
	runtime::Ptr<KnownObject> knownObject = knownObjects.get(targetObjectId);
	return !knownObject ? runtime::Ptr<VisibleObject>() : knownObject->get();
}

runtime::Ptr<model::gameobjects::player::Player> KnownList::getPlayer(int32_t targetObjectId) {
	runtime::Ptr<KnownObject> knownObject = knownObjects.get(targetObjectId);
	return knownObject ? runtime::as<model::gameobjects::player::Player>(knownObject->get()) : runtime::Ptr<model::gameobjects::player::Player>();
}

void KnownList::forEach(const std::function<void(KnownObject&)>& action) {
	utils::collections::CollectionUtil::forEach(
		knownObjects.values(), [&action](const runtime::Ptr<KnownObject>& object) { action(*object); },
		[this]() -> std::string { return "KnownList owner: " + owner.toString(); });
}

void KnownList::forEachObject(const std::function<void(VisibleObject&)>& consumer) {
	forEach([&consumer](KnownObject& object) { consumer(*object.get()); });
}

void KnownList::forEachNpc(const std::function<void(model::gameobjects::Npc&)>& consumer) {
	forEach([&consumer](KnownObject& o) {
		if (auto npc = runtime::as<model::gameobjects::Npc>(o.get()))
			consumer(*npc);
	});
}

void KnownList::forEachPlayer(const std::function<void(model::gameobjects::player::Player&)>& consumer) {
	forEach([&consumer](KnownObject& object) {
		if (auto player = runtime::as<model::gameobjects::player::Player>(object.get()))
			consumer(*player);
	});
}

std::vector<runtime::Ptr<KnownObject>> KnownList::stream() {
	return knownObjects.values().toVector();
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> KnownList::streamPlayers() {
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> players;
	for (runtime::Ptr<KnownObject> o : stream()) {
		if (auto player = runtime::as<model::gameobjects::player::Player>(o->get()))
			players.push_back(player);
	}
	return players;
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> KnownList::streamVisiblePlayers() {
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> players;
	for (runtime::Ptr<KnownObject> o : stream()) {
		if (!o->isVisible())
			continue;
		if (auto player = runtime::as<model::gameobjects::player::Player>(o->get()))
			players.push_back(player);
	}
	return players;
}

} // namespace aion::gameserver::world::knownlist
