#include "aion/gameserver/skillengine/effect/DiseaseEffect.h"

#include <optional>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void DiseaseEffect::calculate(model::Effect& effect) const {
	EffectTemplate::calculate(effect, gameserver::model::stats::container::StatEnum::DISEASE_RESISTANCE, std::nullopt);
}

// skillId 18386
void DiseaseEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void DiseaseEffect::startEffect(model::Effect& effect) const {
	effect.setAbnormal(AbnormalState::DISEASE);
	effect.getEffected()->getEffectController()->setAbnormal(AbnormalState::DISEASE);
}

void DiseaseEffect::endEffect(model::Effect& effect) const {
	if (effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::DISEASE))
		effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::DISEASE);
}

} // namespace aion::gameserver::skillengine::effect
