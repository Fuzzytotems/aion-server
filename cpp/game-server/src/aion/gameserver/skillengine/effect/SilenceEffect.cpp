#include "aion/gameserver/skillengine/effect/SilenceEffect.h"

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

using gameserver::model::gameobjects::Creature;
using runtime::Ptr;

void SilenceEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void SilenceEffect::calculate(model::Effect& effect) const {
	EffectTemplate::calculate(effect, gameserver::model::stats::container::StatEnum::SILENCE_RESISTANCE, std::nullopt);
}

void SilenceEffect::startEffect(model::Effect& effect) const {
	const Ptr<Creature> effected = effect.getEffected();
	effect.setAbnormal(AbnormalState::SILENCE);
	effected->getEffectController()->setAbnormal(AbnormalState::SILENCE);
	// java-race: Java reads effected.getCastingSkill() twice, and a cast may end between the two reads (the second then throws Java's
	// NullPointerException, here the Ptr's); the port reads it twice as well
	if (effected->getCastingSkill() && effected->getCastingSkill()->getSkillTemplate()->getType() == model::SkillType::MAGICAL)
		effected->getController().cancelCurrentSkill(effect.getEffector());
}

void SilenceEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::SILENCE);
}

} // namespace aion::gameserver::skillengine::effect
