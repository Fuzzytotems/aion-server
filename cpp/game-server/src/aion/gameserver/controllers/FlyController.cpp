#include "aion/gameserver/controllers/FlyController.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::controllers {

FlyController::FlyController(model::gameobjects::player::Player& playerValue) : OwnedPart(playerValue), player(playerValue) {
}

FlyController::~FlyController() = default;

void FlyController::onStopGliding() {
	AION_UNPORTED();
}

void FlyController::endFly(bool broadcastPacket) {
	AION_UNPORTED();
}

bool FlyController::startFly(bool broadcastPacket, bool ignoreFlightCooldown) {
	AION_UNPORTED();
}

bool FlyController::canFly(model::gameobjects::player::Player& value) {
	AION_UNPORTED();
}

bool FlyController::switchToGliding() {
	AION_UNPORTED();
}

bool FlyController::canGlide(model::gameobjects::player::Player& value) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers
