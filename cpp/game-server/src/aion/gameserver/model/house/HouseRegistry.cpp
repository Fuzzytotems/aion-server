#include "aion/gameserver/model/house/HouseRegistry.h"

#include "aion/gameserver/model/gameobjects/HouseDecoration.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::house {

HouseRegistry::HouseRegistry(House& ownerValue) : owner(ownerValue) {
}

HouseRegistry::~HouseRegistry() = default;

runtime::Ref<HouseRegistry> HouseRegistry::create(House& ownerValue) {
	return runtime::makeRef<HouseRegistry>(ownerValue);
}

std::vector<runtime::Ptr<gameobjects::HouseObject>> HouseRegistry::getObjects() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::HouseObject>> HouseRegistry::getSpawnedObjects() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::HouseObject>> HouseRegistry::getNotSpawnedObjects() {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::HouseObject> HouseRegistry::getObjectByObjId(int32_t itemObjId) {
	AION_UNPORTED();
}

bool HouseRegistry::putObject(gameobjects::HouseObject& houseObject, bool saveRegistry) {
	AION_UNPORTED();
}

void HouseRegistry::discardObject(gameobjects::HouseObject& object, bool direct) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::HouseDecoration>> HouseRegistry::getDecors() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::HouseDecoration>> HouseRegistry::getUnusedDecors() {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::HouseDecoration> HouseRegistry::getDecorByObjId(int32_t itemObjId) {
	AION_UNPORTED();
}

bool HouseRegistry::putDecor(gameobjects::HouseDecoration& decor, bool saveRegistry) {
	AION_UNPORTED();
}

std::optional<int32_t> HouseRegistry::getUsedDecorId(templates::housing::PartType partType, int32_t room) {
	AION_UNPORTED();
}

void HouseRegistry::setUsed(gameobjects::HouseDecoration& decor, int32_t room) {
	AION_UNPORTED();
}

void HouseRegistry::discardDecor(templates::housing::PartType partType, int32_t roomNo) {
	AION_UNPORTED();
}

void HouseRegistry::discardDecor(gameobjects::HouseDecoration& decor, bool direct) {
	AION_UNPORTED();
}

void HouseRegistry::reset() {
	AION_UNPORTED();
}

void HouseRegistry::save() {
	AION_UNPORTED();
}

int32_t HouseRegistry::size() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::house
