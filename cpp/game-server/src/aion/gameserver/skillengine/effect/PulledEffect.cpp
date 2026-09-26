#include "aion/gameserver/skillengine/effect/PulledEffect.h"

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
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"
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

void PulledEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void PulledEffect::calculate(model::Effect& effect) const {
	const Ptr<controllers::effect::EffectController> ec = effect.getEffected()->getEffectController();
	if (ec->isAbnormalSet(AbnormalState::PULLED) || ec->isAbnormalSet(AbnormalState::STUMBLE) || ec->isAbnormalSet(AbnormalState::OPENAERIAL))
		return;
	if (!world::geo::GeoService::getInstance().canSee(*effect.getEffected(), *effect.getEffector())) {
		return;
	}
	if (!EffectTemplate::calculate(effect, StatEnum::PULLED_RESISTANCE, std::nullopt))
		return;
	if (effect.isSubEffect())
		effect.setSubEffectType(runtime::as<Player>(effect.getEffected()) ? model::SubEffectType::PULL : model::SubEffectType::PULL_NPC);
	const Ptr<Creature> effector = effect.isReflected() ? effect.getOriginalEffected() : effect.getEffector();
	// Target must be pulled just one meter away from effector, not IN place of effector
	double radian = toRadians(PositionUtil::convertHeadingToAngle(PositionUtil::getHeadingTowards(*effector, *effect.getEffected())));
	float z = effector->getZ();
	// Java `(float) Math.cos(radian) * 1.5f`: the cast binds to the cosine, so the product is a float product
	const float x1 = static_cast<float>(std::cos(radian)) * 1.5f;
	const float y1 = static_cast<float>(std::sin(radian)) * 1.5f;
	geoEngine::math::Vector3f closestCollision =
		world::geo::GeoService::getInstance().getClosestCollision(*effect.getEffected(), effector->getX() + x1, effector->getY() + y1, z);
	effect.setTargetLoc(closestCollision.getX(), closestCollision.getY(), closestCollision.getZ());
}

void PulledEffect::startEffect(model::Effect& effect) const {
	const Ptr<Creature> effected = effect.getEffected();
	if (!effect.isReflected()) {
		effected->getController().cancelCurrentSkill(effect.getEffector());
		if (Ptr<Player> player = runtime::as<Player>(effected)) {
			player->getFlyController().onStopGliding();
			player->getController().onStopMove();
		}
	}
	world::World::getInstance().updatePosition(*effected, effect.getTargetX(), effect.getTargetY(), effect.getTargetZ(), effected->getHeading());
	if (runtime::as<Player>(effected))
		utils::PacketSendUtility::broadcastPacketAndReceive(*effected,
			network::aion::serverpackets::SM_FORCED_MOVE(effect.isReflected() ? *effect.getOriginalEffected() : *effect.getEffector(),
				effected->getObjectId(), effect.getTargetX(), effect.getTargetY(), effect.getTargetZ()));
	effect.getEffected()->getEffectController()->setAbnormal(AbnormalState::PULLED);
	effect.setAbnormal(AbnormalState::PULLED);
}

void PulledEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::PULLED);
}

} // namespace aion::gameserver::skillengine::effect
