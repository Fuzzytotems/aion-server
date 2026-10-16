#include "aion/gameserver/skillengine/effect/SwitchHostileEffect.h"

#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;

void SwitchHostileEffect::applyEffect(model::Effect& effect) const {
	Ptr<Creature> effector = effect.getEffector();
	Ptr<Player> player = runtime::as<Player>(effector);
	Ptr<Creature> summon = player ? Ptr<Creature>(player->getSummon()) : nullptr;
	if (summon != nullptr) {
		controllers::attack::AggroList& aggroList = effect.getEffected()->getAggroList();
		int32_t playerHate = aggroList.getHate(*effector);
		int32_t summonHate = aggroList.getHate(*summon);
		aggroList.stopHating(*summon);
		aggroList.stopHating(*effector);
		aggroList.addHate(*effector, summonHate);
		aggroList.addHate(*summon, playerHate);
	}
}

} // namespace aion::gameserver::skillengine::effect
