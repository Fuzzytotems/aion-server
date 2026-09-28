#include "aion/gameserver/skillengine/effect/StaggerEffect.h"

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

void StaggerEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void StaggerEffect::startEffect(model::Effect& effect) const {
	const Ptr<Creature> effected = effect.getEffected();
	effected->getController().cancelCurrentSkill(effect.getEffector());
	effected->getEffectController()->removeParalyzeEffects();
	if (Ptr<Player> player = runtime::as<Player>(effected)) {
		player->getFlyController().onStopGliding();
		player->getController().onStopMove();
	}
	world::World::getInstance().updatePosition(*effected, effect.getTargetX(), effect.getTargetY(), effect.getTargetZ(), effected->getHeading());
	if (runtime::as<Player>(effected))
		utils::PacketSendUtility::broadcastPacketAndReceive(*effected, network::aion::serverpackets::SM_FORCED_MOVE(*effect.getEffector(),
			effected->getObjectId(), effect.getTargetX(), effect.getTargetY(), effect.getTargetZ()));
	effect.getEffected()->getEffectController()->setAbnormal(AbnormalState::STAGGER);
	effect.setAbnormal(AbnormalState::STAGGER);
}

void StaggerEffect::calculate(model::Effect& effect) const {
	if (effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::PULLED)
		|| effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::STAGGER)
		|| effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::OPENAERIAL)
		|| effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::STUMBLE))
		return;

	if (!EffectTemplate::calculate(effect, StatEnum::STAGGER_RESISTANCE, model::SpellStatus::STAGGER))
		return;
	if (effect.isSubEffect() && !runtime::as<Player>(effect.getEffected()))
		effect.setSubEffectType(model::SubEffectType::STAGGER);
	const Ptr<Creature> effector = effect.getEffector();
	const Ptr<Creature> effected = effect.getEffected();
	// Move effected 2 meters backward as on retail
	double radian = toRadians(PositionUtil::convertHeadingToAngle(PositionUtil::getHeadingTowards(*effector, *effect.getEffected())));
	float x1 = static_cast<float>(std::cos(radian) * 2);
	float y1 = static_cast<float>(std::sin(radian) * 2);
	geoEngine::math::Vector3f closestCollision =
		world::geo::GeoService::getInstance().getClosestCollision(*effected, effected->getX() + x1, effected->getY() + y1, effected->getZ());
	effect.setTargetLoc(closestCollision.getX(), closestCollision.getY(), closestCollision.getZ());
}

void StaggerEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::STAGGER);
}

} // namespace aion::gameserver::skillengine::effect
