#include "aion/gameserver/skillengine/effect/SkillLauncherEffect.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void SkillLauncherEffect::applyEffect(model::Effect& effect) const {
	SkillEngine::getInstance().applyEffect(skillId, *effect.getEffector(), *effect.getEffected());
}

void SkillLauncherEffect::calculate(model::Effect& effect) const {
	effect.addSuccessEffect(this);
}

} // namespace aion::gameserver::skillengine::effect
