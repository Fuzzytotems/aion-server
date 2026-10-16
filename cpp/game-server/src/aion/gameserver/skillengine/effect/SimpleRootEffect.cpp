#include "aion/gameserver/skillengine/effect/SimpleRootEffect.h"

#include <cmath>
#include <cstdint>
#include <optional>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/network/aion/serverpackets/SM_POSITION.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
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
using runtime::Ptr;
using utils::PositionUtil;

namespace {

/** Java Math.toRadians(double) (JDK 9+: angdeg * DEGREES_TO_RADIANS) */
constexpr double toRadians(double angdeg) noexcept {
	return angdeg * 0.017453292519943295;
}

} // namespace

void SimpleRootEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void SimpleRootEffect::calculate(model::Effect& effect) const {
	if (effect.getEffected()->getEffectController()->isInAnyAbnormalState(AbnormalState::CANT_MOVE_STATE))
		return;
	if (EffectTemplate::calculate(effect, gameserver::model::stats::container::StatEnum::STAGGER_RESISTANCE, std::nullopt) && effect.isSubEffect()) {
		effect.setSubEffectType(model::SubEffectType::SIMPLE_MOVE_BACK);
		const Ptr<Creature> effected = effect.getEffected();
		int8_t heading = PositionUtil::getHeadingTowards(*effect.getEffector(), *effect.getEffected());
		double radian = toRadians(PositionUtil::convertHeadingToAngle(heading));
		// Java `(float) (Math.cos(radian) * 0.7f)`: a double product narrowed to float
		float x1 = static_cast<float>(std::cos(radian) * 0.7f);
		float y1 = static_cast<float>(std::sin(radian) * 0.7f);
		geoEngine::math::Vector3f closestCollision =
			world::geo::GeoService::getInstance().getClosestCollision(*effected, effected->getX() + x1, effected->getY() + y1, effected->getZ());
		effect.setTargetLoc(closestCollision.getX(), closestCollision.getY(), closestCollision.getZ());
	}
}

void SimpleRootEffect::startEffect(model::Effect& effect) const {
	const Ptr<Creature> effected = effect.getEffected();
	effect.setSpellStatus(model::SpellStatus::NONE);
	if (Ptr<Player> player = runtime::as<Player>(effected))
		player->getController().onStopMove();
	if (effect.isSubEffect()) {
		world::World::getInstance().updatePosition(*effected, effect.getTargetX(), effect.getTargetY(), effect.getTargetZ(), effected->getHeading(),
			false);
		if (!runtime::as<Player>(effected))
			utils::PacketSendUtility::broadcastPacket(*effected, network::aion::serverpackets::SM_POSITION(*effected));
	}
	effect.getEffected()->getEffectController()->setAbnormal(AbnormalState::SIMPLE_MOVE_BACK);
	effect.setAbnormal(AbnormalState::SIMPLE_MOVE_BACK);
}

void SimpleRootEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::SIMPLE_MOVE_BACK);
}

} // namespace aion::gameserver::skillengine::effect
