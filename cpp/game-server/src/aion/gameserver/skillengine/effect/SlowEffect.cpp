#include "aion/gameserver/skillengine/effect/SlowEffect.h"

#include <optional>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::stats::container::StatEnum;

void SlowEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void SlowEffect::calculate(model::Effect& effect) const {
	BufEffect::calculate(effect, StatEnum::SLOW_RESISTANCE, std::nullopt);
}

void SlowEffect::startEffect(model::Effect& effect) const {
	BufEffect::startEffect(effect);
	effect.setAbnormal(AbnormalState::SLOW);
	effect.getEffected()->getEffectController()->setAbnormal(AbnormalState::SLOW);
}

void SlowEffect::endEffect(model::Effect& effect) const {
	BufEffect::endEffect(effect);
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::SLOW);
}

} // namespace aion::gameserver::skillengine::effect
