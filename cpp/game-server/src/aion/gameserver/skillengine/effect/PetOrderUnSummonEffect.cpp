#include "aion/gameserver/skillengine/effect/PetOrderUnSummonEffect.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/summons/UnsummonType.h"
#include "aion/gameserver/services/summons/SummonsService.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

// Java PetOrderUnSummonEffect.java:23-32
void PetOrderUnSummonEffect::applyEffect(model::Effect& effect) const {
	runtime::Ptr<gameserver::model::gameobjects::Creature> effected = effect.getEffected();
	if (runtime::Ptr<gameserver::model::gameobjects::player::Player> player = runtime::as<gameserver::model::gameobjects::player::Player>(effected)) {
		runtime::Ptr<gameserver::model::gameobjects::Summon> summon = player->getSummon();
		if (summon) {
			services::summons::SummonsService::release(*summon, gameserver::model::summons::UnsummonType::PET_ORDER_UNSUMMON_EFFECT);
		}
	}
}

} // namespace aion::gameserver::skillengine::effect
