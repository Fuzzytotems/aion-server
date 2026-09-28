#include "aion/gameserver/services/conquerorAndProtectorSystem/CPBuff.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::conquerorAndProtectorSystem {

CPBuff::CPBuff() {
}

runtime::Ref<CPBuff> CPBuff::create() {
	return runtime::makeRef<CPBuff>();
}

void CPBuff::applyEffect(model::gameobjects::player::Player& player, model::templates::cp::CPType type, int32_t rank) {
	AION_UNPORTED();
}

void CPBuff::endEffect(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

CPBuff::~CPBuff() = default;

} // namespace aion::gameserver::services::conquerorAndProtectorSystem
