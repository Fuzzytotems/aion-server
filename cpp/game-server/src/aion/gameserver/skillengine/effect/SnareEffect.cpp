#include "aion/gameserver/skillengine/effect/SnareEffect.h"

#include <optional>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::stats::container::StatEnum;

void SnareEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void SnareEffect::calculate(model::Effect& effect) const {
	BufEffect::calculate(effect, StatEnum::SNARE_RESISTANCE, std::nullopt);
}

void SnareEffect::endEffect(model::Effect& effect) const {
	BufEffect::endEffect(effect);
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::SNARE);
}

void SnareEffect::startEffect(model::Effect& effect) const {
	BufEffect::startEffect(effect);
	effect.getEffected()->getEffectController()->setAbnormal(AbnormalState::SNARE);
	effect.setAbnormal(AbnormalState::SNARE);
}

} // namespace aion::gameserver::skillengine::effect
