#include "aion/gameserver/skillengine/effect/BuffSleepEffect.h"

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void BuffSleepEffect::calculate(model::Effect& effect) const {
	effect.addSuccessEffect(this);
}

void BuffSleepEffect::startEffect(model::Effect& effect) const {
	const runtime::Ptr<gameserver::model::gameobjects::Creature> effected = effect.getEffected();
	effected->getController().cancelCurrentSkill(effect.getEffector());
	effect.setAbnormal(AbnormalState::SLEEP);
	effected->getEffectController()->setAbnormal(AbnormalState::SLEEP);
}

} // namespace aion::gameserver::skillengine::effect
