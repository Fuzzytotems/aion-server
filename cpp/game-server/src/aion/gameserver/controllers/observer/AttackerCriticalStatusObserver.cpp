#include "aion/gameserver/controllers/observer/AttackerCriticalStatusObserver.h"

#include "aion/gameserver/controllers/observer/AttackerCriticalStatus.h"

namespace aion::gameserver::controllers::observer {

AttackerCriticalStatusObserver::AttackerCriticalStatusObserver(attack::AttackStatus statusValue, int32_t count, int32_t valueValue,
	bool isPercent)
	: status(statusValue) {
	acStatus.set(AttackerCriticalStatus::create(count, valueValue, isPercent));
}

AttackerCriticalStatusObserver::~AttackerCriticalStatusObserver() = default;

runtime::Ref<AttackerCriticalStatusObserver> AttackerCriticalStatusObserver::create(attack::AttackStatus statusValue, int32_t count,
	int32_t valueValue, bool isPercent) {
	return runtime::makeRef<AttackerCriticalStatusObserver>(statusValue, count, valueValue, isPercent);
}

int32_t AttackerCriticalStatusObserver::getCount() {
	return acStatus->getCount();
}

void AttackerCriticalStatusObserver::decreaseCount() {
	acStatus->setCount((acStatus->getCount() - 1)); // java-race: read-modify-write without a lock, as in Java
}

} // namespace aion::gameserver::controllers::observer
