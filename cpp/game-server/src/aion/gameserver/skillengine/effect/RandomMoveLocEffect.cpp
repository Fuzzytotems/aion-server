#include "aion/gameserver/skillengine/effect/RandomMoveLocEffect.h"

#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/DashStatus.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using runtime::Ptr;

void RandomMoveLocEffect::applyEffect(model::Effect& effect) const {
	const Ptr<model::Skill> skill = effect.getSkill();
	world::World::getInstance().updatePosition(*effect.getEffector(), skill->getX(), skill->getY(), skill->getZ(), skill->getH());
	if (Ptr<controllers::movement::PlayerMoveController> pmc =
			runtime::as<controllers::movement::PlayerMoveController>(effect.getEffector()->getMoveController()))
		pmc->setHasMovedByRandomMoveLocEffect(*skill);
}

void RandomMoveLocEffect::calculate(model::Effect& effect) const {
	effect.addSuccessEffect(this);
	model::DashStatus ds = reserved5 == 1 ? model::DashStatus::RANDOMMOVELOC_NEW : model::DashStatus::RANDOMMOVELOC;
	effect.setDashStatus(ds);
	const Ptr<Creature> effector = effect.getEffector();
	// Move Effector backwards direction=1 or frontwards direction=0
	float dir = utils::PositionUtil::convertHeadingToAngle(effector->getHeading());
	geoEngine::math::Vector3f closestCollision =
		world::geo::GeoService::getInstance().findMovementCollision(*effector, direction == 1 ? dir + 180 : dir, distance);
	effect.getSkill()->setTargetPosition(closestCollision.getX(), closestCollision.getY(), closestCollision.getZ(), effector->getHeading());
}

} // namespace aion::gameserver::skillengine::effect
