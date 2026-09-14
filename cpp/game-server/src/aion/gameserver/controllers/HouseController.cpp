#include "aion/gameserver/controllers/HouseController.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/house/House.h"

namespace aion::gameserver::controllers {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.controllers.HouseController");

HouseController::HouseController() = default;

HouseController::~HouseController() = default;

model::house::House& HouseController::getOwner() const {
	return static_cast<model::house::House&>(VisibleObjectController::getOwner());
}

void HouseController::see(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

void HouseController::spawnObjects() {
	AION_UNPORTED();
}

void HouseController::onAfterSpawn() {
	AION_UNPORTED();
}

void HouseController::updateSpawns() {
	AION_UNPORTED();
}

void HouseController::onDespawn() {
	AION_UNPORTED();
}

void HouseController::updateAppearance() {
	AION_UNPORTED();
}

// callbacks: the forEachPlayer lambda (HouseController.java:118) runs during the call
void HouseController::kickVisitors(runtime::Ptr<model::gameobjects::player::Player> kicker, bool kickFriends, bool ownerChanged) {
	AION_UNPORTED();
}

void HouseController::moveOutside(model::gameobjects::player::Player& player, bool ownerChanged) {
	AION_UNPORTED();
}

void HouseController::teleportNearHouseDoor(model::gameobjects::player::Player& player, bool outsideHouse) {
	AION_UNPORTED();
}

void HouseController::updateSign() {
	AION_UNPORTED();
}

void HouseController::updateHouseSpawns() {
	AION_UNPORTED();
}

int32_t HouseController::getCurrentSignNpcId() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers
