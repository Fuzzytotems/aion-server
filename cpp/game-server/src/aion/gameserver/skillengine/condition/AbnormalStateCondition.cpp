#include "aion/gameserver/skillengine/condition/AbnormalStateCondition.h"

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::condition {

bool AbnormalStateCondition::validate(model::Skill& env) const {
	if (env.getFirstTarget())
		return (env.getFirstTarget()->getEffectController()->isAbnormalSet(value));
	return false;
}

bool AbnormalStateCondition::validate(model::Effect& effect) const {
	if (effect.getEffected())
		return (effect.getEffected()->getEffectController()->isAbnormalSet(value));
	return false;
}

} // namespace aion::gameserver::skillengine::condition
