#include "aion/gameserver/skillengine/effect/ResurrectPositionalEffect.h"

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

void ResurrectPositionalEffect::applyEffect(model::Effect& effect) const {
	// Java: (Player) casts - a ClassCastException for another creature; calculate admits players only
	Player& effector = *runtime::cast<Player>(effect.getEffector());
	Player& effected = *runtime::cast<Player>(effect.getEffected());
	effected.setPlayerResActivate(true);
	effected.setResurrectionSkill(skillId);
	utils::PacketSendUtility::sendPacket(effected, network::aion::serverpackets::SM_RESURRECT(*effect.getEffector(), effect.getSkillId()));
	effected.setResPosState(true);
	effected.setResPosX(effector.getX());
	effected.setResPosY(effector.getY());
	effected.setResPosZ(effector.getZ());
}

void ResurrectPositionalEffect::calculate(model::Effect& effect) const {
	if (runtime::as<Player>(effect.getEffector()) && runtime::as<Player>(effect.getEffected()) && effect.getEffected()->isDead())
		EffectTemplate::calculate(effect, std::nullopt, std::nullopt);
}

} // namespace aion::gameserver::skillengine::effect
