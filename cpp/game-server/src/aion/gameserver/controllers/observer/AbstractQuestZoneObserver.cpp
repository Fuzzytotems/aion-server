#include "aion/gameserver/controllers/observer/AbstractQuestZoneObserver.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::controllers::observer {

AbstractQuestZoneObserver::AbstractQuestZoneObserver(model::gameobjects::player::Player& playerValue,
	const model::templates::zone::ZoneTemplate* zoneTemplate)
	: ActionObserver(ObserverType::ALL), player(playerValue), startPos(playerValue.getX(), playerValue.getY(), playerValue.getZ()),
	  startTime(commons::utils::currentTimeMillis()), observedZone(zoneTemplate) {
	oldPos.set(startPos); // Java: startPos.clone()
}

AbstractQuestZoneObserver::~AbstractQuestZoneObserver() = default;

// callbacks: com.aionemu.gameserver.controllers.observer.AbstractQuestZoneObserver$1 (execute(), fieldmap AbstractQuestZoneObserver_Runnable)
void AbstractQuestZoneObserver::moved() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::observer
