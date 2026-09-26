#include "aion/gameserver/controllers/observer/DeathObserver.h"

#include <utility>

#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::controllers::observer {

DeathObserver::DeathObserver(runtime::PinnedCallback<void(model::gameobjects::Creature&)> actionOnDeathValue)
	: ActionObserver(ObserverType::DEATH), actionOnDeath(std::move(actionOnDeathValue)) {
}

DeathObserver::~DeathObserver() = default;

runtime::Ref<DeathObserver> DeathObserver::create(runtime::PinnedCallback<void(model::gameobjects::Creature&)> actionOnDeathValue) {
	return runtime::makeRef<DeathObserver>(std::move(actionOnDeathValue));
}

void DeathObserver::died(model::gameobjects::Creature& lastAttacker) {
	actionOnDeath(lastAttacker);
}

} // namespace aion::gameserver::controllers::observer
