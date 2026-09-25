#include "aion/gameserver/skillengine/effect/DeformEffect.h"

#include <optional>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void DeformEffect::applyEffect(model::Effect& effect) const {
	TransformEffect::applyEffect(effect);
}

void DeformEffect::calculate(model::Effect& effect) const {
	EffectTemplate::calculate(effect, gameserver::model::stats::container::StatEnum::DEFORM_RESISTANCE, std::nullopt);
}

void DeformEffect::startEffect(model::Effect& effect) const {
	TransformEffect::startEffect(effect);
	effect.getEffected()->getEffectController()->setAbnormal(AbnormalState::DEFORM);
	effect.setAbnormal(AbnormalState::DEFORM);
}

void DeformEffect::endEffect(model::Effect& effect) const {
	TransformEffect::endEffect(effect);
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::DEFORM);
}

} // namespace aion::gameserver::skillengine::effect
