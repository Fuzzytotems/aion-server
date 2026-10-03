#include "aion/gameserver/skillengine/effect/InvulnerableWingEffect.h"

#include <optional>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::player::Player;

void InvulnerableWingEffect::calculate(model::Effect& effect) const {
	// Only for players
	if (runtime::as<Player>(effect.getEffected()))
		EffectTemplate::calculate(effect, std::nullopt, std::nullopt);
}

void InvulnerableWingEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
	effect.getEffected()->getEffectController()->setAbnormal(AbnormalState::INVULNERABLE_WING);
}

void InvulnerableWingEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::INVULNERABLE_WING);
}

} // namespace aion::gameserver::skillengine::effect
