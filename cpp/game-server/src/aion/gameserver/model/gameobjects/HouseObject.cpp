#include "aion/gameserver/model/gameobjects/HouseObject.h"

#include <optional>
#include <string_view>

#include "aion/gameserver/controllers/PlaceableObjectController.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HousingObjectData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/housing/PlaceableHouseObject.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_EDIT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"

namespace aion::gameserver::model::gameobjects {

namespace {

/** Java `DataManager.HOUSING_OBJECT_DATA.getTemplateById(templateId)` in the super(...) call (a null result is Java's null template) */
const templates::VisibleObjectTemplate* housingObjectTemplateOf(int32_t templateId) {
	return dataholders::DataManager::HOUSING_OBJECT_DATA->getTemplateById(templateId);
}

/** Java reads the nullable template attribute directly; a missing one is Java's NullPointerException at the caller */
template <class T>
T requireTemplateValue(const std::optional<T>& value, std::string_view attribute) {
	if (!value)
		throw runtime::NullPointerException("PlaceableHouseObject." + std::string(attribute));
	return *value;
}

} // namespace

HouseObject::HouseObject(CreateKey key, house::HouseRegistry& value, int32_t objId, int32_t templateId)
	: HouseObject(key, value, objId, templateId, false) {
}

HouseObject::HouseObject(CreateKey key, runtime::Ptr<house::HouseRegistry> value, int32_t objId, int32_t templateId, bool autoReleaseObjectId)
	: VisibleObject(key, objId, std::make_unique<controllers::PlaceableObjectController>(), nullptr, housingObjectTemplateOf(templateId), nullptr,
		  autoReleaseObjectId),
	  registry(value) {
	getController().setOwner(*this); // binds the late-bound part before publication (no virtual call on the owner)
	setKnownlist(std::make_unique<world::knownlist::PlayerAwareKnownList>(*this));
}

void HouseObject::setPersistentState(Persistable::PersistentState value) {
	// java-race: check-then-act with the DAO save thread (PlayerRegisteredItemsDAO stores the object and then sets UPDATED), so a change made in between
	// is marked saved until the next change
	switch (value) {
		case PersistentState::DELETED:
			if (persistentState.get() == PersistentState::NEW)
				persistentState.set(PersistentState::NOACTION);
			else if (persistentState.get() != PersistentState::DELETED) {
				persistentState.set(PersistentState::DELETED);
				registry->setPersistentState(PersistentState::UPDATE_REQUIRED);
			}
			break;
		case PersistentState::UPDATE_REQUIRED:
			if (persistentState.get() == PersistentState::NEW) {
				registry->setPersistentState(PersistentState::UPDATE_REQUIRED);
				break;
			}
			[[fallthrough]];
		default:
			if (persistentState.get() != value) {
				persistentState.set(value);
				registry->setPersistentState(PersistentState::UPDATE_REQUIRED);
			}
	}
}

void HouseObject::onExpire(player::Player& player) {
	despawnAndRemoveHouseObject(player, true);
}

void HouseObject::despawnAndRemoveHouseObject(player::Player& player, bool isExpired) {
	if (isSpawnedByPlayer()) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_HOUSE_EDIT(7, 0, getObjectId()));
		getController().delete_();
	}
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_HOUSE_EDIT(4, 1, getObjectId()));
	if (isExpired)
		utils::PacketSendUtility::sendPacket(player,
			network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_DELETE_EXPIRE_TIME(getObjectTemplate()->getL10n()));
	else
		utils::PacketSendUtility::sendPacket(player,
			network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_DELETE_USE_COUNT_FINAL(getObjectTemplate()->getL10n()));
	registry->discardObject(*this, false);

	// if owner is not online, we should save his items
	runtime::Ptr<player::Player> owner = world::World::getInstance().getPlayer(registry->getOwner()->getOwnerId());
	if (!owner || !owner->isOnline())
		registry->save();
}

const templates::housing::PlaceableHouseObject* HouseObject::getObjectTemplate() const {
	return static_cast<const templates::housing::PlaceableHouseObject*>(VisibleObject::getObjectTemplate());
}

void HouseObject::setX(float value) {
	if (x.get() != value) {
		x.set(value);
		setPersistentState(PersistentState::UPDATE_REQUIRED);
		if (runtime::Ptr<world::WorldPosition> current = position.get())
			current->setXYZH(value, std::nullopt, std::nullopt, std::nullopt);
	}
}

void HouseObject::setY(float value) {
	if (y.get() != value) {
		y.set(value);
		setPersistentState(PersistentState::UPDATE_REQUIRED);
		if (runtime::Ptr<world::WorldPosition> current = position.get())
			current->setXYZH(std::nullopt, value, std::nullopt, std::nullopt);
	}
}

void HouseObject::setZ(float value) {
	if (z.get() != value) {
		z.set(value);
		setPersistentState(PersistentState::UPDATE_REQUIRED);
		if (runtime::Ptr<world::WorldPosition> current = position.get())
			current->setXYZH(std::nullopt, std::nullopt, value, std::nullopt);
	}
}

void HouseObject::setHeading(int8_t value) {
	if (heading.get() != value) {
		heading.set(value);
		setPersistentState(PersistentState::UPDATE_REQUIRED);
		if (runtime::Ptr<world::WorldPosition> current = position.get())
			current->setXYZH(std::nullopt, std::nullopt, std::nullopt, value);
	}
}

int32_t HouseObject::getRotation() {
	int32_t rotation = heading.get() & 0xFF;
	return rotation * 3;
}

void HouseObject::setRotation(int32_t rotation) {
	setHeading(utils::PositionUtil::convertAngleToHeading(static_cast<float>(rotation)));
}

templates::housing::PlaceLocation HouseObject::getPlaceLocation() {
	return requireTemplateValue(getObjectTemplate()->getLocation(), "location");
}

templates::housing::PlaceArea HouseObject::getPlaceArea() {
	return requireTemplateValue(getObjectTemplate()->getArea(), "area");
}

int32_t HouseObject::getPlacementLimit(bool trial) {
	// Java: getObjectTemplate().getPlacementLimit() and LimitType.get(Trial)ObjectPlaceLimit(registry.getOwner().getBuilding().getSize()):
	// PlaceableHouseObject and Building declare no such accessors and LimitType has no companion yet (P4-07b)
	static_cast<void>(trial);
	AION_UNPORTED();
}

templates::item::ItemQuality HouseObject::getQuality() {
	return getObjectTemplate()->getQuality();
}

float HouseObject::getTalkingDistance() {
	return getObjectTemplate()->getTalkingDistance();
}

templates::housing::HousingCategory HouseObject::getCategory() {
	return getObjectTemplate()->getCategory();
}

runtime::Ptr<house::House> HouseObject::getOwnerHouse() {
	return registry->getOwner();
}

int32_t HouseObject::getPlayerId() {
	return registry->getOwner()->getOwnerId();
}

void HouseObject::incrementOwnerUsedCount() {
	ownerUsedCount++;
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void HouseObject::incrementVisitorUsedCount() {
	visitorUsedCount++;
	setPersistentState(PersistentState::UPDATE_REQUIRED);
	runtime::Ptr<player::Player> owner = world::World::getInstance().getPlayer(registry->getOwner()->getOwnerId());
	// if owner is not online, we should save his items
	if (!owner || !owner->isOnline())
		registry->save();
}

void HouseObject::setOwnerUsedCount(int32_t value) {
	if (ownerUsedCount.get() != value) {
		ownerUsedCount.set(value);
		setPersistentState(PersistentState::UPDATE_REQUIRED);
	}
}

void HouseObject::setVisitorUsedCount(int32_t value) {
	if (visitorUsedCount.get() != value) {
		visitorUsedCount.set(value);
		setPersistentState(PersistentState::UPDATE_REQUIRED);
	}
}

bool HouseObject::isSpawnedByPlayer() {
	return x.get() != 0 || y.get() != 0 || z.get() != 0;
}

controllers::PlaceableObjectController& HouseObject::getController() const {
	return static_cast<controllers::PlaceableObjectController&>(VisibleObject::getController());
}

void HouseObject::spawn() {
	if (!isSpawnedByPlayer())
		return;
	if (!position.get() || !isSpawned()) {
		runtime::Ptr<house::House> owner = registry->getOwner();
		position.set(world::World::getInstance().createPosition(owner->getWorldId(), x.get(), y.get(), z.get(), heading.get(), owner->getInstanceId()));
		spawnengine::SpawnEngine::bringIntoWorld(*this);
	} else {
		updateKnownlist();
	}
}

void HouseObject::removeFromHouse() {
	getController().delete_();
	heading.set(0); // Java: x = y = z = heading = 0
	z.set(0);
	y.set(0);
	x.set(0);
	position.set(nullptr);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void HouseObject::onUse(player::Player& player) {
}

void HouseObject::onDialogRequest(player::Player& player) {
	onUse(player);
}

void HouseObject::onDespawn() {
}

void HouseObject::setColor(std::optional<int32_t> value) {
	color.set(value);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

} // namespace aion::gameserver::model::gameobjects
