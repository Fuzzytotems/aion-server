#include "aion/gameserver/controllers/observer/DeathObserver.h"

#include <utility>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"

namespace aion::gameserver::controllers::observer {

DeathObserver::DeathObserver(runtime::PinnedCallback<void(model::gameobjects::Creature&)> actionOnDeathValue)
	: ActionObserver(ObserverType::DEATH), actionOnDeath(std::move(actionOnDeathValue)) {
}

DeathObserver::~DeathObserver() = default;

runtime::Ref<DeathObserver> DeathObserver::create(runtime::PinnedCallback<void(model::gameobjects::Creature&)> actionOnDeathValue) {
	return runtime::makeRef<DeathObserver>(std::move(actionOnDeathValue));
}

void DeathObserver::died(model::gameobjects::Creature& lastAttacker) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::observer
