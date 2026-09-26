#include "aion/gameserver/skillengine/effect/modifier/TargetClassDamageModifier.h"

#include <cstdint>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect::modifier {

int32_t TargetClassDamageModifier::analyze(model::Effect& effect) const {
	runtime::Ptr<gameserver::model::gameobjects::Creature> effected = effect.getEffected();
	if (runtime::Ptr<gameserver::model::gameobjects::player::Player> player = runtime::as<gameserver::model::gameobjects::player::Player>(effected)) {
		// Java: player.getPlayerClass() == skillTargetClass - an enum reference compare, false for a null attribute
		if (player->getPlayerClass() == skillTargetClass) {
			// Java: value + effect.getSkillLevel() * delta - int arithmetic, wrapping on overflow
			return static_cast<int32_t>(static_cast<uint32_t>(value) + static_cast<uint32_t>(effect.getSkillLevel()) * static_cast<uint32_t>(delta));
		}
	}
	return 0;
}

bool TargetClassDamageModifier::check(model::Effect& effect) const {
	runtime::Ptr<gameserver::model::gameobjects::Creature> effected = effect.getEffected();
	if (runtime::Ptr<gameserver::model::gameobjects::player::Player> player = runtime::as<gameserver::model::gameobjects::player::Player>(effected)) {
		return player->getPlayerClass() == skillTargetClass;
	}
	return false;
}

} // namespace aion::gameserver::skillengine::effect::modifier
