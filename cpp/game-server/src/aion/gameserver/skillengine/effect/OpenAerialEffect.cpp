#include "aion/gameserver/skillengine/effect/OpenAerialEffect.h"

#include <cmath>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORCED_MOVE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SpellStatus.h"
#include "aion/gameserver/skillengine/model/SubEffectType.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::stats::container::StatEnum;
using runtime::Ptr;

void OpenAerialEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void OpenAerialEffect::calculate(model::Effect& effect) const {
	if (effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::PULLED)
		|| effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::STUMBLE)
		|| effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::OPENAERIAL)
		|| effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::STAGGER)
		|| effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::SPIN))
		return;
	if (!EffectTemplate::calculate(effect, StatEnum::OPENAERIAL_RESISTANCE, model::SpellStatus::OPENAERIAL))
		return;
	if (effect.isSubEffect() && !runtime::as<Player>(effect.getEffected()))
		effect.setSubEffectType(model::SubEffectType::OPENAERIAL);
	const Ptr<Creature> effected = effect.getEffected();
	float z = effected->getZ();
	if (!effected->isFlying()) {
		float geoZ = world::geo::GeoService::getInstance().getZ(effected->getWorldId(), effected->getX(), effected->getY(), effected->getZ() + 2,
			effected->getZ() - 1, effected->getInstanceId());
		if (!std::isnan(geoZ)) {
			z = geoZ;
		}
	}
	effect.setTargetLoc(effected->getX(), effected->getY(), z);
}

void OpenAerialEffect::startEffect(model::Effect& effect) const {
	const Ptr<Creature> effected = effect.getEffected();
	effected->getController().cancelCurrentSkill(effect.getEffector());
	effect.getEffected()->getEffectController()->removeParalyzeEffects();
	if (Ptr<Player> player = runtime::as<Player>(effected)) {
		player->getFlyController().onStopGliding();
		player->getController().onStopMove();
	}
	world::World::getInstance().updatePosition(*effected, effect.getTargetX(), effect.getTargetY(), effect.getTargetZ(), effected->getHeading());
	if (runtime::as<Player>(effected))
		utils::PacketSendUtility::broadcastPacketAndReceive(*effected, network::aion::serverpackets::SM_FORCED_MOVE(*effect.getEffector(),
			effected->getObjectId(), effect.getTargetX(), effect.getTargetY(), effect.getTargetZ()));
	effect.setAbnormal(AbnormalState::OPENAERIAL);
	effected->getEffectController()->setAbnormal(AbnormalState::OPENAERIAL);
}

void OpenAerialEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::OPENAERIAL);
}

} // namespace aion::gameserver::skillengine::effect
