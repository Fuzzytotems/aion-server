#include "aion/gameserver/skillengine/effect/SummonTrapEffect.h"

#include <cmath>
#include <cstdint>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntention.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/geoEngine/collision/IgnoreProperties.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Trap.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/summons/TrapService.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::skillengine::effect {

// Stored lambdas of the Java class (hub-headers.md §7.3): SummonTrapEffect@L48 - the trap's delete, pinned on the trap and stored as its
// controller's DESPAWN task

namespace {

/** Java Math.toRadians(double) (JDK 9+: angdeg * DEGREES_TO_RADIANS) */
constexpr double toRadians(double angdeg) noexcept {
	return angdeg * 0.017453292519943295;
}

} // namespace

void SummonTrapEffect::applyEffect(model::Effect& effect) const {
	gameserver::model::gameobjects::Creature& effector = *effect.getEffector();
	// should only be set if player has no target to avoid errors
	if (!effect.getEffector()->getTarget())
		effect.getEffector()->setTarget(effect.getEffector());
	double radian = toRadians(utils::PositionUtil::convertHeadingToAngle(effect.getEffector()->getHeading()));
	float x = effect.getX();
	float y = effect.getY();
	float z = effect.getZ();
	if (effect.getSkill()->isFirstTargetSelf()) {
		gameserver::model::gameobjects::Creature& effected = *effect.getEffected();
		geoEngine::math::Vector3f pos = world::geo::GeoService::getInstance().getClosestCollision(effector,
			effected.getX() + static_cast<float>(std::cos(radian) * 2), effected.getY() + static_cast<float>(std::sin(radian) * 2), effected.getZ(),
			true, getId(geoEngine::collision::CollisionIntention::DEFAULT_COLLISIONS),
			*geoEngine::collision::IgnoreProperties::of(effector.getRace()));
		x = pos.getX();
		y = pos.getY();
		z = pos.getZ();
	}
	int8_t heading = effector.getHeading();
	int32_t worldId = effector.getWorldId();
	int32_t instanceId = effector.getInstanceId();

	runtime::Ref<gameserver::model::templates::spawns::SpawnTemplate> spawn =
		spawnengine::SpawnEngine::newSingleTimeSpawn(worldId, npcId, x, y, z, heading);
	runtime::Ref<gameserver::model::gameobjects::Trap> trap = spawnengine::VisibleObjectSpawner::spawnTrap(*spawn, instanceId, effector);
	services::summons::TrapService::registerTrap(effector.getObjectId(), trap, true);
	gameserver::model::gameobjects::Trap& placed = *trap;
	placed.getController().addTask(gameserver::model::TaskId::DESPAWN,
		utils::ThreadPoolManager::getInstance().schedule({&placed}, [&placed] { placed.getController().delete_(); }, time * int64_t{1000}));
}

} // namespace aion::gameserver::skillengine::effect
