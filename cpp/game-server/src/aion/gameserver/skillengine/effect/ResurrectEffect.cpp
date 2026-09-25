#include "aion/gameserver/skillengine/effect/ResurrectEffect.h"

#include <optional>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RESURRECT.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;

void ResurrectEffect::applyEffect(model::Effect& effect) const {
	if (Ptr<Player> effectedPlayer = runtime::as<Player>(effect.getEffected())) {
		effectedPlayer->setPlayerResActivate(true);
		effectedPlayer->setResurrectionSkill(skillId);
		utils::PacketSendUtility::sendPacket(*effectedPlayer, network::aion::serverpackets::SM_RESURRECT(*effect.getEffector(), effect.getSkillId()));
	}
}

void ResurrectEffect::calculate(model::Effect& effect) const {
	if (runtime::as<Player>(effect.getEffected()) && effect.getEffected()->isDead())
		EffectTemplate::calculate(effect, std::nullopt, std::nullopt);
}

} // namespace aion::gameserver::skillengine::effect
