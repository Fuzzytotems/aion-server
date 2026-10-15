#include "aion/gameserver/skillengine/effect/ResurrectBaseEffect.h"

#include <optional>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;

void ResurrectBaseEffect::calculate(model::Effect& effect) const {
	EffectTemplate::calculate(effect, std::nullopt, std::nullopt);
}

void ResurrectBaseEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void ResurrectBaseEffect::endEffect(model::Effect& effect) const {
	runtime::Ptr<Creature> effected = effect.getEffected();
	runtime::Ptr<Player> player = runtime::as<Player>(effected);
	if (effected->isDead() && player && !player->isDueling(*effect.getEffector()))
		services::player::PlayerReviveService::scheduleReviveAtBase(*player, 2500, skillId);
}

} // namespace aion::gameserver::skillengine::effect
