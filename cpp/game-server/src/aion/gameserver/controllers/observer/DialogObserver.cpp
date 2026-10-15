#include "aion/gameserver/controllers/observer/DialogObserver.h"

#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::controllers::observer {

DialogObserver::DialogObserver(model::gameobjects::Creature& requesterValue, model::gameobjects::player::Player& responderValue,
	int32_t maxDistanceValue)
	: ActionObserver(ObserverType::MOVE), responder(responderValue), requester(requesterValue), maxDistance(maxDistanceValue) {
}

DialogObserver::~DialogObserver() = default;

void DialogObserver::moved() {
	if (!utils::PositionUtil::isInRange(*responder, *requester, static_cast<float>(maxDistance)))
		tooFar();
}

} // namespace aion::gameserver::controllers::observer
