#include "aion/gameserver/controllers/PetController.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::controllers {

PetController::PetUpdateTask::PetUpdateTask(model::gameobjects::player::Player& playerValue) : player(playerValue) {
}

PetController::PetUpdateTask::~PetUpdateTask() = default;

runtime::Ref<PetController::PetUpdateTask> PetController::PetUpdateTask::create(model::gameobjects::player::Player& playerValue) {
	return runtime::makeRef<PetUpdateTask>(playerValue);
}

void PetController::PetUpdateTask::run() {
	AION_UNPORTED();
}

PetController::PetController() = default;

PetController::~PetController() = default;

model::gameobjects::Pet& PetController::getOwner() const {
	return static_cast<model::gameobjects::Pet&>(VisibleObjectController::getOwner());
}

void PetController::onDelete() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers
