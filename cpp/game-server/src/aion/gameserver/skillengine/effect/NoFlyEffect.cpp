#include "aion/gameserver/skillengine/effect/NoFlyEffect.h"

#include <optional>

#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::player::Player;
using gameserver::model::stats::container::StatEnum;

void NoFlyEffect::calculate(model::Effect& effect) const {
	EffectTemplate::calculate(effect, StatEnum::NOFLY_RESISTANCE, std::nullopt);
}

void NoFlyEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

bool NoFlyEffect::isDodgedOrResisted(model::Effect& effect, std::optional<StatEnum> statEnum) const {
	if (effect.getEffected()->getEffectController()->isInAnyAbnormalState(AbnormalState::INVULNERABLE_WING)) {
		return true;
	}
	return EffectTemplate::isDodgedOrResisted(effect, statEnum);
}

void NoFlyEffect::startEffect(model::Effect& effect) const {
	if (runtime::Ptr<Player> player = runtime::as<Player>(effect.getEffected())) {
		player->getFlyController().endFly(true);
	}
	effect.setAbnormal(AbnormalState::NOFLY);
	effect.getEffected()->getEffectController()->setAbnormal(AbnormalState::NOFLY);
}

void NoFlyEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::NOFLY);
}

} // namespace aion::gameserver::skillengine::effect
