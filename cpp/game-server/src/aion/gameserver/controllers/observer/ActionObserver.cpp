#include "aion/gameserver/controllers/observer/ActionObserver.h"

namespace aion::gameserver::controllers::observer {

ActionObserver::ActionObserver(ObserverType observerTypeValue) : observerType(observerTypeValue) {
}

ActionObserver::~ActionObserver() = default;

runtime::Ref<ActionObserver> ActionObserver::create(ObserverType observerTypeValue) {
	return runtime::makeRef<ActionObserver>(observerTypeValue);
}

} // namespace aion::gameserver::controllers::observer
