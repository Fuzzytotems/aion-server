#include "aion/gameserver/controllers/observer/AttackCalcObserver.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::controllers::observer {

AttackCalcObserver::AttackCalcObserver() = default;

AttackCalcObserver::~AttackCalcObserver() = default;

runtime::Ref<AttackCalcObserver> AttackCalcObserver::create() {
	return runtime::makeRef<AttackCalcObserver>();
}

runtime::Ref<AttackerCriticalStatus> AttackCalcObserver::checkAttackerCriticalStatus(attack::AttackStatus status, bool isSkill) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::observer
