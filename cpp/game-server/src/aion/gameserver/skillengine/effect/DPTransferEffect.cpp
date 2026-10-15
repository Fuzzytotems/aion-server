#include "aion/gameserver/skillengine/effect/DPTransferEffect.h"

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::player::Player;

void DPTransferEffect::applyEffect(model::Effect& effect) const {
	int32_t newValue = effect.getReserveds(position)->getValue();
	runtime::cast<Player>(effect.getEffected())->getCommonData()->addDp(newValue);
	runtime::cast<Player>(effect.getEffector())->getCommonData()->addDp(-newValue);
}

void DPTransferEffect::calculate(model::Effect& effect) const {
	if (!EffectTemplate::calculate(effect, std::nullopt, std::nullopt))
		return;
	effect.setReserveds(*model::EffectReserved::create(position, getCurrentStatValue(effect), model::EffectReserved::ResourceType::DP, true), false);
}

int32_t DPTransferEffect::getCurrentStatValue(model::Effect& effect) const {
	return runtime::cast<Player>(effect.getEffector())->getCommonData()->getDp();
}

} // namespace aion::gameserver::skillengine::effect
