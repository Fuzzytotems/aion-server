#include "aion/gameserver/controllers/observer/AttackCalcObserver.h"

#include "aion/gameserver/controllers/observer/AttackerCriticalStatus.h"

namespace aion::gameserver::controllers::observer {

AttackCalcObserver::AttackCalcObserver() = default;

AttackCalcObserver::~AttackCalcObserver() = default;

runtime::Ref<AttackCalcObserver> AttackCalcObserver::create() {
	return runtime::makeRef<AttackCalcObserver>();
}

runtime::Ref<AttackerCriticalStatus> AttackCalcObserver::checkAttackerCriticalStatus(attack::AttackStatus status, bool isSkill) {
	return AttackerCriticalStatus::create(false);
}

} // namespace aion::gameserver::controllers::observer
