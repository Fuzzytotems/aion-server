#include "aion/gameserver/skillengine/effect/StatupEffect.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void StatupEffect::endEffect(model::Effect& effect) const {
	BufEffect::endEffect(effect);
	effect.getEffected()->getLifeStats()->updateCurrentStats();
}

} // namespace aion::gameserver::skillengine::effect
