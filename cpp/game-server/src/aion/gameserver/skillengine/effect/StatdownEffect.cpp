#include "aion/gameserver/skillengine/effect/StatdownEffect.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void StatdownEffect::startEffect(model::Effect& effect) const {
	BufEffect::startEffect(effect);
	effect.getEffected()->getLifeStats()->updateCurrentStats();
}

// TODO bosses are resistent to this?

} // namespace aion::gameserver::skillengine::effect
