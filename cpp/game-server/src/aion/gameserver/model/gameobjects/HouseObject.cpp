#include "aion/gameserver/model/gameobjects/HouseObject.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/templates/housing/PlaceableHouseObject.h"
#include "aion/gameserver/controllers/PlaceableObjectController.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::model::gameobjects {

HouseObject::HouseObject(CreateKey key, house::HouseRegistry& value, int32_t objId, int32_t templateId)
	: HouseObject(key, value, objId, templateId, false) {
}

HouseObject::HouseObject(CreateKey key, runtime::Ptr<house::HouseRegistry> value, int32_t objId, int32_t templateId,
	bool autoReleaseObjectId)
	: VisibleObject(key, int32_t{}, nullptr, runtime::Ptr<templates::spawns::SpawnTemplate>{},
	static_cast<const templates::VisibleObjectTemplate*>(nullptr), runtime::Ptr<world::WorldPosition>{}, bool{}), registry(value) {
	// Java: super(objId, new PlaceableObjectController<T>(), null, DataManager.HOUSING_OBJECT_DATA.getTemplateById(templateId), null,
	// autoReleaseObjectId); getController().setOwner(this); setKnownlist(new PlayerAwareKnownList(this)); super(...) arguments
	AION_UNPORTED();
}

void HouseObject::setPersistentState(Persistable::PersistentState value) {
	AION_UNPORTED();
}

void HouseObject::onExpire(player::Player& player) {
	AION_UNPORTED();
}

void HouseObject::despawnAndRemoveHouseObject(player::Player& player, bool isExpired) {
	AION_UNPORTED();
}

const templates::housing::PlaceableHouseObject* HouseObject::getObjectTemplate() const {
	return static_cast<const templates::housing::PlaceableHouseObject*>(VisibleObject::getObjectTemplate());
}

void HouseObject::setX(float value) {
	AION_UNPORTED();
}

void HouseObject::setY(float value) {
	AION_UNPORTED();
}

void HouseObject::setZ(float value) {
	AION_UNPORTED();
}

void HouseObject::setHeading(int8_t value) {
	AION_UNPORTED();
}

int32_t HouseObject::getRotation() {
	AION_UNPORTED();
}

void HouseObject::setRotation(int32_t rotation) {
	AION_UNPORTED();
}

templates::housing::PlaceLocation HouseObject::getPlaceLocation() {
	AION_UNPORTED();
}

templates::housing::PlaceArea HouseObject::getPlaceArea() {
	AION_UNPORTED();
}

int32_t HouseObject::getPlacementLimit(bool trial) {
	AION_UNPORTED();
}

templates::item::ItemQuality HouseObject::getQuality() {
	AION_UNPORTED();
}

float HouseObject::getTalkingDistance() {
	AION_UNPORTED();
}

templates::housing::HousingCategory HouseObject::getCategory() {
	AION_UNPORTED();
}

runtime::Ptr<house::House> HouseObject::getOwnerHouse() {
	AION_UNPORTED();
}

int32_t HouseObject::getPlayerId() {
	AION_UNPORTED();
}

void HouseObject::incrementOwnerUsedCount() {
	AION_UNPORTED();
}

void HouseObject::incrementVisitorUsedCount() {
	AION_UNPORTED();
}

void HouseObject::setOwnerUsedCount(int32_t value) {
	AION_UNPORTED();
}

void HouseObject::setVisitorUsedCount(int32_t value) {
	AION_UNPORTED();
}

bool HouseObject::isSpawnedByPlayer() {
	AION_UNPORTED();
}

controllers::PlaceableObjectController& HouseObject::getController() const {
	return static_cast<controllers::PlaceableObjectController&>(VisibleObject::getController());
}

void HouseObject::spawn() {
	AION_UNPORTED();
}

void HouseObject::removeFromHouse() {
	AION_UNPORTED();
}

void HouseObject::onUse(player::Player& player) {
	AION_UNPORTED();
}

void HouseObject::onDialogRequest(player::Player& player) {
	AION_UNPORTED();
}

void HouseObject::onDespawn() {
	AION_UNPORTED();
}

void HouseObject::setColor(std::optional<int32_t> value) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects
