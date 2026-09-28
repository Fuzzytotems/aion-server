#include "aion/gameserver/model/event/Headhunter.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::event {

Headhunter::Headhunter(int32_t value, int32_t accumulatedKillsValue, int64_t lastUpdateValue, gameobjects::Persistable::PersistentState stateValue)
	: state(stateValue), hunterId(value), accumulatedKills(accumulatedKillsValue), lastUpdate(lastUpdateValue) {
}

runtime::Ref<Headhunter> Headhunter::create(int32_t value, int32_t accumulatedKillsValue, int64_t lastUpdateValue,
	gameobjects::Persistable::PersistentState stateValue) {
	return runtime::makeRef<Headhunter>(value, accumulatedKillsValue, lastUpdateValue, stateValue);
}

int32_t Headhunter::incrementAndGetKills() {
	AION_UNPORTED();
}

void Headhunter::setPersistentState(gameobjects::Persistable::PersistentState value) {
	state.set(value);
}

int32_t Headhunter::compareTo(const Headhunter& hunter) const {
	AION_UNPORTED();
}

Headhunter::~Headhunter() = default;

} // namespace aion::gameserver::model::event
