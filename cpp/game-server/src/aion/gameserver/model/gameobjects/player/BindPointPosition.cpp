#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"

namespace aion::gameserver::model::gameobjects::player {

BindPointPosition::BindPointPosition(int32_t mapIdValue, float xValue, float yValue, float zValue, int8_t headingValue)
	: mapId(mapIdValue), x(xValue), y(yValue), z(zValue), heading(headingValue), persistentState(PersistentState::NEW) {
}

BindPointPosition::~BindPointPosition() = default;

runtime::Ref<BindPointPosition> BindPointPosition::create(int32_t mapIdValue, float xValue, float yValue, float zValue, int8_t headingValue) {
	return runtime::makeRef<BindPointPosition>(mapIdValue, xValue, yValue, zValue, headingValue);
}

void BindPointPosition::setPersistentState(PersistentState persistentStateValue) {
	// Java: switch with fallthrough, UPDATE_REQUIRED keeps a NEW state
	if (persistentStateValue == PersistentState::UPDATE_REQUIRED && persistentState.get() == PersistentState::NEW)
		return;
	persistentState.set(persistentStateValue);
}

} // namespace aion::gameserver::model::gameobjects::player
