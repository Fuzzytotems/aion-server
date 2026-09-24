#include "aion/gameserver/skillengine/effect/ReturnEffect.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::player::Player;

void ReturnEffect::applyEffect(model::Effect& effect) const {
	services::teleport::TeleportService::moveToBindLocation(*runtime::cast<Player>(effect.getEffector()));
}

void ReturnEffect::calculate(model::Effect& effect) const {
	if (effect.getEffected()->isSpawned())
		effect.addSuccessEffect(this);
}

} // namespace aion::gameserver::skillengine::effect
