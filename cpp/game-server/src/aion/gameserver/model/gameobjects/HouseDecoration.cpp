#include "aion/gameserver/model/gameobjects/HouseDecoration.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::gameobjects {

HouseDecoration::HouseDecoration(int32_t value, int32_t templateIdValue)
	: HouseDecoration(value, templateIdValue, -1) {
}

runtime::Ref<HouseDecoration> HouseDecoration::create(int32_t value, int32_t templateIdValue) {
	return runtime::makeRef<HouseDecoration>(value, templateIdValue);
}

HouseDecoration::HouseDecoration(int32_t value, int32_t templateIdValue, int32_t roomValue)
	: AionObject(value), templateId(templateIdValue), room(static_cast<int8_t>(roomValue)), persistentState(Persistable::PersistentState::NEW) {
}

runtime::Ref<HouseDecoration> HouseDecoration::create(int32_t value, int32_t templateIdValue, int32_t roomValue) {
	return runtime::makeRef<HouseDecoration>(value, templateIdValue, roomValue);
}

const templates::housing::HousePart* HouseDecoration::getTemplate() {
	AION_UNPORTED();
}

void HouseDecoration::setPersistentState(Persistable::PersistentState value) {
	persistentState.set(value);
}

std::string HouseDecoration::getName() {
	AION_UNPORTED();
}

void HouseDecoration::setRoom(int32_t value) {
	AION_UNPORTED();
}

HouseDecoration::~HouseDecoration() = default;

} // namespace aion::gameserver::model::gameobjects
