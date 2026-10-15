#include "aion/gameserver/skillengine/effect/ProcDPHealInstantEffect.h"

#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/HealType.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::player::Player;

void ProcDPHealInstantEffect::calculate(model::Effect& effect) const {
	AbstractHealEffect::calculate(effect, model::HealType::DP);
}

void ProcDPHealInstantEffect::applyEffect(model::Effect& effect) const {
	AbstractHealEffect::applyEffect(effect, model::HealType::DP);
}
int32_t ProcDPHealInstantEffect::getCurrentStatValue(model::Effect& effect) const {
	return runtime::cast<Player>(effect.getEffected())->getCommonData()->getDp();
}

int32_t ProcDPHealInstantEffect::getMaxStatValue(model::Effect& effect) const {
	return runtime::cast<Player>(effect.getEffected())->getGameStats()->getMaxDp()->getCurrent();
}

} // namespace aion::gameserver::skillengine::effect
