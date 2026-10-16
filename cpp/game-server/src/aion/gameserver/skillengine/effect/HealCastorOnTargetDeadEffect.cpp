#include "aion/gameserver/skillengine/effect/HealCastorOnTargetDeadEffect.h"

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;
using runtime::Ptr;

void HealCastorOnTargetDeadEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void HealCastorOnTargetDeadEffect::endEffect(model::Effect& effect) const {
	const Ptr<Creature> effected = effect.getEffected();
	const Ptr<Creature> effector = effect.getEffector();
	if (effected->isDead()) {
		int32_t healValue = calculateBaseValue(effect);
		const Ptr<Player> player = runtime::as<Player>(effector);
		const Ptr<gameserver::model::team::TemporaryPlayerTeam> group = healparty && player ? player->getCurrentGroup() : nullptr;
		if (group == nullptr) {
			if (utils::PositionUtil::isInRange(*effected, *effector, range, false))
				effector->getLifeStats()->increaseHp(SM_ATTACK_STATUS_TYPE::HP, healValue, effect, SM_ATTACK_STATUS_LOG::REGULAR);
		} else {
			// Java heals the effector once per member in range (HealCastorOnTargetDeadEffect.java:43-46), not the member: kept as Java has it
			for (const Ptr<Player>& p : group->getOnlineMembers()) {
				if (utils::PositionUtil::isInRange(*effected, *p, range, false))
					effector->getLifeStats()->increaseHp(SM_ATTACK_STATUS_TYPE::HP, healValue, effect, SM_ATTACK_STATUS_LOG::REGULAR);
			}
		}
	}
}

} // namespace aion::gameserver::skillengine::effect
