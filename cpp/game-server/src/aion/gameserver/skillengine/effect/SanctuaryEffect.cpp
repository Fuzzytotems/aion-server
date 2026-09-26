#include "aion/gameserver/skillengine/effect/SanctuaryEffect.h"

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void SanctuaryEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
	if (effect.getEffector()->equals(*effect.getEffected()))
		effect.getEffected()->setTarget(effect.getEffected());
}

void SanctuaryEffect::startEffect(model::Effect& effect) const {
	effect.setAbnormal(AbnormalState::SANCTUARY);
	effect.getEffected()->getEffectController()->setAbnormal(AbnormalState::SANCTUARY);
}

void SanctuaryEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::SANCTUARY);
}

} // namespace aion::gameserver::skillengine::effect
