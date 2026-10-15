#include "aion/gameserver/model/gameobjects/UseableHouseObject.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/HouseRegistry.h"

namespace aion::gameserver::model::gameobjects {

UseableHouseObject::UseableHouseObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId)
	: HouseObject(key, registry, objId, templateId) {
}

UseableHouseObject::~UseableHouseObject() = default;

bool UseableHouseObject::canExpireNow() {
	return !isOccupied();
}

bool UseableHouseObject::isOccupied() {
	return usingPlayer.get() != 0;
}

bool UseableHouseObject::setOccupant(player::Player& player) {
	return usingPlayer.compareAndSet(0, player.getObjectId()) || usingPlayer.get() == player.getObjectId();
}

bool UseableHouseObject::releaseOccupant(player::Player& player) {
	return usingPlayer.compareAndSet(player.getObjectId(), 0);
}

void UseableHouseObject::releaseOccupant() {
	usingPlayer.set(0);
}

bool UseableHouseObject::hasUseCooldown() {
	return false;
}

} // namespace aion::gameserver::model::gameobjects
