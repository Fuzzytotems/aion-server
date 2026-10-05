#include "aion/gameserver/skillengine/effect/RecallInstantEffect.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/services/RecallService.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::effect {

void RecallInstantEffect::applyEffect(model::Effect& effect) const {
	runtime::Ptr<gameserver::model::gameobjects::player::Player> caster = runtime::as<gameserver::model::gameobjects::player::Player>(effect.getEffector());
	runtime::Ptr<gameserver::model::gameobjects::player::Player> effected = runtime::as<gameserver::model::gameobjects::player::Player>(effect.getEffected());
	if (caster && effected)
		services::RecallService::getInstance().requestSummon(*caster, *effected, effect.getSkillId());
}

void RecallInstantEffect::calculate(model::Effect& effect) const {
	runtime::Ptr<gameserver::model::gameobjects::Creature> effector = effect.getEffector();
	runtime::Ptr<gameserver::model::gameobjects::Creature> effected = effect.getEffected();
	// Java: canBeSummoned(effector, effect.getEffected()) answers false for a null effected (its instanceof check)
	if (effected && services::RecallService::canBeSummoned(*effector, *effected)) {
		effect.getSkill()->setTargetPosition(effector->getX(), effector->getY(), effector->getZ(), effector->getHeading());
		effect.addSuccessEffect(this);
	}
}

} // namespace aion::gameserver::skillengine::effect
