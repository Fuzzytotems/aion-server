#include "aion/gameserver/controllers/observer/AttackStatusObserver.h"

#include "aion/gameserver/controllers/observer/AttackerCriticalStatus.h"

namespace aion::gameserver::controllers::observer {

AttackStatusObserver::AttackStatusObserver(int32_t valueValue, attack::AttackStatus statusValue) : value(valueValue), status(statusValue) {
}

AttackStatusObserver::~AttackStatusObserver() = default;

runtime::Ref<AttackStatusObserver> AttackStatusObserver::create(int32_t valueValue, attack::AttackStatus statusValue) {
	return runtime::makeRef<AttackStatusObserver>(valueValue, statusValue);
}

} // namespace aion::gameserver::controllers::observer
