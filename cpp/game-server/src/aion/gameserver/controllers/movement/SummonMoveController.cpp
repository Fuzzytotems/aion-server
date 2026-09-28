#include "aion/gameserver/controllers/movement/SummonMoveController.h"

#include "aion/gameserver/model/gameobjects/Summon.h"

namespace aion::gameserver::controllers::movement {

SummonMoveController::SummonMoveController(model::gameobjects::Summon& ownerValue) : PlayableMoveController(ownerValue) {
}

SummonMoveController::~SummonMoveController() = default;

} // namespace aion::gameserver::controllers::movement
