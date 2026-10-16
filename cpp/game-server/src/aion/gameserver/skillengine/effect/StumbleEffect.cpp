#include "aion/gameserver/skillengine/effect/StumbleEffect.h"

#include <cmath>
#include <optional>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORCED_MOVE.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SpellStatus.h"
#include "aion/gameserver/skillengine/model/SubEffectType.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::stats::container::StatEnum;
using runtime::Ptr;
using utils::PositionUtil;

namespace {

/** Java Math.toRadians(double) (JDK 9+: angdeg * DEGREES_TO_RADIANS) */
constexpr double toRadians(double angdeg) noexcept {
	return angdeg * 0.017453292519943295;
}

} // namespace

void StumbleEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void StumbleEffect::startEffect(model::Effect& effect) const {
	const Ptr<Creature> effected = effect.getEffected();
	effected->getController().cancelCurrentSkill(effect.getEffector());
	effected->getEffectController()->removeParalyzeEffects();
	effected->getEffectController()->removeStunEffects();
	if (Ptr<Player> player = runtime::as<Player>(effected)) {
		player->getFlyController().onStopGliding();
		player->getController().onStopMove();
	}
	world::World::getInstance().updatePosition(*effected, effect.getTargetX(), effect.getTargetY(), effect.getTargetZ(), effected->getHeading());
	// TODO: FI_RobustCrash_G1 or FI_Whirlwind_G1 don't send anything, find pattern
	if (runtime::as<Player>(effected))
		utils::PacketSendUtility::broadcastPacketAndReceive(*effected, network::aion::serverpackets::SM_FORCED_MOVE(*effect.getEffector(),
			effected->getObjectId(), effect.getTargetX(), effect.getTargetY(), effect.getTargetZ()));
	effect.getEffected()->getEffectController()->setAbnormal(AbnormalState::STUMBLE);
	effect.setAbnormal(AbnormalState::STUMBLE);
}

void StumbleEffect::calculate(model::Effect& effect) const {
	if (effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::OPENAERIAL)
		|| effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::PULLED)
		|| effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::STUMBLE)
		|| effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::STAGGER)) {
		return;
	}

	if (!EffectTemplate::calculate(effect, StatEnum::STUMBLE_RESISTANCE, model::SpellStatus::STUMBLE))
		return;
	if (effect.isSubEffect() && !runtime::as<Player>(effect.getEffected()))
		effect.setSubEffectType(model::SubEffectType::STUMBLE);
	const Ptr<Creature> effector = effect.getEffector();
	const Ptr<Creature> effected = effect.getEffected();
	double radian = toRadians(PositionUtil::convertHeadingToAngle(PositionUtil::getHeadingTowards(*effector, *effect.getEffected())));
	float x1 = static_cast<float>(std::cos(radian) * 2);
	float y1 = static_cast<float>(std::sin(radian) * 2);
	geoEngine::math::Vector3f closestCollision =
		world::geo::GeoService::getInstance().getClosestCollision(*effected, effected->getX() + x1, effected->getY() + y1, effected->getZ());
	effect.setTargetLoc(closestCollision.getX(), closestCollision.getY(), closestCollision.getZ());
}

void StumbleEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::STUMBLE);
}

} // namespace aion::gameserver::skillengine::effect
