#include "aion/gameserver/skillengine/effect/DelayedSkillEffect.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void DelayedSkillEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void DelayedSkillEffect::endEffect(model::Effect& effect) const {
	EffectTemplate::endEffect(effect);
	if (effect.isEndedByTime())
		SkillEngine::getInstance().applyEffectsDirectly(skillId, *effect.getEffector(), *effect.getEffected(), effect.getTargetX(), effect.getTargetY(),
			effect.getTargetZ());
}

} // namespace aion::gameserver::skillengine::effect
