#include "aion/gameserver/skillengine/effect/CurseEffect.h"

#include <optional>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void CurseEffect::calculate(model::Effect& effect) const {
	EffectTemplate::calculate(effect, gameserver::model::stats::container::StatEnum::CURSE_RESISTANCE, std::nullopt);
}

void CurseEffect::startEffect(model::Effect& effect) const {
	BufEffect::startEffect(effect);
	effect.setAbnormal(AbnormalState::CURSE);
	effect.getEffected()->getEffectController()->setAbnormal(AbnormalState::CURSE);
}

void CurseEffect::endEffect(model::Effect& effect) const {
	BufEffect::endEffect(effect);
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::CURSE);
}

} // namespace aion::gameserver::skillengine::effect
