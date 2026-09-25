#include "aion/gameserver/skillengine/effect/FallEffect.h"

#include <optional>

#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::player::Player;

bool FallEffect::isDodgedOrResisted(model::Effect& effect, std::optional<gameserver::model::stats::container::StatEnum> statEnum) const {
	if (effect.getEffected()->getEffectController()->isInAnyAbnormalState(AbnormalState::INVULNERABLE_WING)) {
		return true;
	}
	return EffectTemplate::isDodgedOrResisted(effect, statEnum);
}

void FallEffect::applyEffect(model::Effect& effect) const {
	if (runtime::Ptr<Player> player = runtime::as<Player>(effect.getEffected())) {
		player->getFlyController().endFly(true);
	}
}

} // namespace aion::gameserver::skillengine::effect
