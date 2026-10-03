#include "aion/gameserver/skillengine/effect/SwitchHpMpEffect.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void SwitchHpMpEffect::applyEffect(model::Effect& effect) const {
	runtime::Ptr<gameserver::model::stats::container::CreatureLifeStats> lifeStats = effect.getEffected()->getLifeStats();
	int32_t currentHp = lifeStats->getCurrentHp();
	int32_t currentMp = lifeStats->getCurrentMp();
	// doesn't send sm_attack_status, checked on 4.5
	lifeStats->setCurrentHp(currentMp, *effect.getEffector());
	lifeStats->setCurrentMp(currentHp);
}

} // namespace aion::gameserver::skillengine::effect
