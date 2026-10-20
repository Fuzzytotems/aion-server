#include "aion/gameserver/model/house/HouseRegistry.h"

#include "aion/gameserver/dao/PlayerRegisteredItemsDAO.h"
#include "aion/gameserver/model/gameobjects/HouseDecoration.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/UseableHouseObject.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/templates/housing/Building.h"
#include "aion/gameserver/model/templates/housing/HousePart.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::model::house {

HouseRegistry::HouseRegistry(House& ownerValue) : owner(ownerValue) {
}

HouseRegistry::~HouseRegistry() = default;

runtime::Ref<HouseRegistry> HouseRegistry::create(House& ownerValue) {
	return runtime::makeRef<HouseRegistry>(ownerValue);
}

// Java HouseRegistry.java:136-146 (the <T extends AionObject & Persistable> helper of discardObject and discardDecor)
template <class T>
void HouseRegistry::discard(runtime::LinkedHashMap<int32_t, runtime::Ref<T>>& map, T& obj, bool direct) {
	if (obj.getPersistentState() == PersistentState::NEW || direct) {
		map.remove(obj.getObjectId(), runtime::Ptr<T>(obj));
	} else {
		obj.setPersistentState(PersistentState::DELETED);
		setPersistentState(PersistentState::UPDATE_REQUIRED);
	}
	// remove house object use cooldowns for this object
	if (runtime::Ptr<gameobjects::UseableHouseObject> useableHouseObject = runtime::as<gameobjects::UseableHouseObject>(obj);
		useableHouseObject && useableHouseObject->hasUseCooldown()) {
		const int32_t objectId = obj.getObjectId();
		// lambda at HouseRegistry.java:145: runs synchronously on this thread, so the id is captured by value
		world::World::getInstance().forEachPlayer(
			[objectId](gameobjects::player::Player& player) { player.getHouseObjectCooldowns()->remove(objectId); });
	}
}

std::vector<runtime::Ptr<gameobjects::HouseObject>> HouseRegistry::getObjects() {
	return objects.values();
}

std::vector<runtime::Ptr<gameobjects::HouseObject>> HouseRegistry::getSpawnedObjects() {
	std::vector<runtime::Ptr<gameobjects::HouseObject>> temp;
	for (runtime::Ptr<gameobjects::HouseObject> obj : objects.values()) {
		if (obj->isSpawnedByPlayer() && obj->getPersistentState() != PersistentState::DELETED)
			temp.push_back(obj);
	}
	return temp;
}

std::vector<runtime::Ptr<gameobjects::HouseObject>> HouseRegistry::getNotSpawnedObjects() {
	std::vector<runtime::Ptr<gameobjects::HouseObject>> temp;
	for (runtime::Ptr<gameobjects::HouseObject> obj : objects.values()) {
		if (!obj->isSpawnedByPlayer() && obj->getPersistentState() != PersistentState::DELETED)
			temp.push_back(obj);
	}
	return temp;
}

runtime::Ptr<gameobjects::HouseObject> HouseRegistry::getObjectByObjId(int32_t itemObjId) {
	return objects.get(itemObjId);
}

bool HouseRegistry::putObject(gameobjects::HouseObject& houseObject, bool saveRegistry) {
	if (objects.putIfAbsent(houseObject.getObjectId(), runtime::Ref<gameobjects::HouseObject>(houseObject)))
		return false;
	if (houseObject.getPersistentState() != PersistentState::UPDATED) // state is UPDATED when reloading registry and spawned objects get reused
		setPersistentState(PersistentState::UPDATE_REQUIRED);
	if (saveRegistry)
		save();
	return true;
}

void HouseRegistry::discardObject(gameobjects::HouseObject& object, bool direct) {
	discard(objects, object, direct);
}

std::vector<runtime::Ptr<gameobjects::HouseDecoration>> HouseRegistry::getDecors() {
	return decors.values();
}

std::vector<runtime::Ptr<gameobjects::HouseDecoration>> HouseRegistry::getUnusedDecors() {
	std::vector<runtime::Ptr<gameobjects::HouseDecoration>> temp;
	for (runtime::Ptr<gameobjects::HouseDecoration> decor : decors.values()) {
		if (decor->getPersistentState() != PersistentState::DELETED && decor->getRoom() == -1)
			temp.push_back(decor);
	}
	return temp;
}

runtime::Ptr<gameobjects::HouseDecoration> HouseRegistry::getDecorByObjId(int32_t itemObjId) {
	return decors.get(itemObjId);
}

bool HouseRegistry::putDecor(gameobjects::HouseDecoration& decor, bool saveRegistry) {
	if (decors.putIfAbsent(decor.getObjectId(), runtime::Ref<gameobjects::HouseDecoration>(decor)))
		return false;
	if (decor.getPersistentState() != PersistentState::UPDATED)
		setPersistentState(PersistentState::UPDATE_REQUIRED);
	if (saveRegistry)
		save();
	return true;
}

std::optional<int32_t> HouseRegistry::getUsedDecorId(templates::housing::PartType partType, int32_t room) {
	for (runtime::Ptr<gameobjects::HouseDecoration> decor : decors.values()) {
		if (decor->getPersistentState() != PersistentState::DELETED && decor->getTemplate()->getType() == partType && decor->getRoom() == room)
			return decor->getTemplateId();
	}
	return getOwner()->getBuilding()->getDefaultDecorId(partType);
}

void HouseRegistry::setUsed(gameobjects::HouseDecoration& decor, int32_t room) {
	if (decor.getPersistentState() == PersistentState::DELETED || decor.getRoom() == room)
		return;
	discardDecor(decor.getTemplate()->getType(), room);
	std::optional<int32_t> defaultPartId = getOwner()->getBuilding()->getDefaultDecorId(decor.getTemplate()->getType());
	if (!defaultPartId) // Java: Integer == int unboxes the Integer
		throw runtime::NullPointerException("Building " + std::to_string(getOwner()->getBuilding()->getId()) + " has no default decoration for the part type");
	if (*defaultPartId == decor.getTemplateId()) {
		decor.setPersistentState(PersistentState::DELETED);
	} else {
		decor.setRoom(room);
		if (decor.getPersistentState() != PersistentState::NEW) {
			decor.setPersistentState(PersistentState::UPDATE_REQUIRED);
			setPersistentState(PersistentState::UPDATE_REQUIRED);
		}
	}
}

void HouseRegistry::discardDecor(templates::housing::PartType partType, int32_t roomNo) {
	for (runtime::Ptr<gameobjects::HouseDecoration> decor : getDecors()) {
		if (decor->getTemplate()->getType() == partType && decor->getRoom() == roomNo)
			discardDecor(*decor, false);
	}
}

void HouseRegistry::discardDecor(gameobjects::HouseDecoration& decor, bool direct) {
	discard(decors, decor, direct);
}

void HouseRegistry::reset() {
	std::vector<runtime::Ptr<gameobjects::HouseObject>> spawnedObjects = getSpawnedObjects();
	if (spawnedObjects.empty()) {
		if (getOwner()->getOwnerId() != 0)
			dao::PlayerRegisteredItemsDAO::resetRegistry(getOwner()->getOwnerId());
	} else {
		for (const runtime::Ptr<gameobjects::HouseObject>& obj : spawnedObjects)
			obj->removeFromHouse();
	}
	for (runtime::Ptr<gameobjects::HouseDecoration> decor : decors.values()) {
		if (decor->getRoom() != -1)
			discardDecor(*decor, false);
	}
	save();
}

void HouseRegistry::save() {
	if (persistentState.get() == PersistentState::UPDATE_REQUIRED)
		dao::PlayerRegisteredItemsDAO::store(*this, getOwner()->getOwnerId());
}

void HouseRegistry::drop() {
	objects.clear();
	decors.clear();
}

int32_t HouseRegistry::size() {
	return objects.size() + decors.size();
}

} // namespace aion::gameserver::model::house
