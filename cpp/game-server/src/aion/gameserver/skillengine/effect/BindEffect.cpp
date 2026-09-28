#include "aion/gameserver/skillengine/effect/BindEffect.h"

#include <optional>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/SkillType.h"

namespace aion::gameserver::skillengine::effect {

void BindEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void BindEffect::calculate(model::Effect& effect) const {
	EffectTemplate::calculate(effect, gameserver::model::stats::container::StatEnum::BIND_RESISTANCE, std::nullopt);
}

void BindEffect::startEffect(model::Effect& effect) const {
	const runtime::Ptr<gameserver::model::gameobjects::Creature> effected = effect.getEffected();
	effect.setAbnormal(AbnormalState::BIND);
	effected->getEffectController()->setAbnormal(AbnormalState::BIND);
	if (effected->getCastingSkill() && effected->getCastingSkill()->getSkillTemplate()->getType() == model::SkillType::PHYSICAL)
		effected->getController().cancelCurrentSkill(effect.getEffector());
}

void BindEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::BIND);
}

} // namespace aion::gameserver::skillengine::effect
