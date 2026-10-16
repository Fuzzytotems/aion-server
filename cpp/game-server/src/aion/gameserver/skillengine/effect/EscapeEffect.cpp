#include "aion/gameserver/skillengine/effect/EscapeEffect.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void EscapeEffect::applyEffect(model::Effect& effect) const {
	services::teleport::TeleportService::moveToBindLocation(*runtime::cast<gameserver::model::gameobjects::player::Player>(effect.getEffector()));
}

void EscapeEffect::calculate(model::Effect& effect) const {
	if (effect.getEffected()->isSpawned())
		effect.addSuccessEffect(this);
}

} // namespace aion::gameserver::skillengine::effect
