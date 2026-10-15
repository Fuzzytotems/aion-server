#include "aion/gameserver/model/event/Headhunter.h"

#include "aion/commons/utils/TimeUtils.h"

namespace aion::gameserver::model::event {

Headhunter::Headhunter(int32_t value, int32_t accumulatedKillsValue, int64_t lastUpdateValue, gameobjects::Persistable::PersistentState stateValue)
	: state(stateValue), hunterId(value), accumulatedKills(accumulatedKillsValue), lastUpdate(lastUpdateValue) {
}

runtime::Ref<Headhunter> Headhunter::create(int32_t value, int32_t accumulatedKillsValue, int64_t lastUpdateValue,
	gameobjects::Persistable::PersistentState stateValue) {
	return runtime::makeRef<Headhunter>(value, accumulatedKillsValue, lastUpdateValue, stateValue);
}

// Java Headhunter.java:37-42
int32_t Headhunter::incrementAndGetKills() {
	accumulatedKills.set(accumulatedKills.get() + 1); // Java: accumulatedKills++ (no atomicity)
	lastUpdate.set(commons::utils::currentTimeMillis());
	state.set(gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
	return accumulatedKills.get();
}

void Headhunter::setPersistentState(gameobjects::Persistable::PersistentState value) {
	state.set(value);
}

// Java Headhunter.java:58-66
int32_t Headhunter::compareTo(const Headhunter& hunter) const {
	if (accumulatedKills.get() > hunter.getKills())
		return -1;
	else if (accumulatedKills.get() < hunter.getKills())
		return 1;
	else // accumulatedKills == hunter.getKills()
		return lastUpdate.get() > hunter.getLastUpdate() ? -1 : 1;
}

Headhunter::~Headhunter() = default;

} // namespace aion::gameserver::model::event
