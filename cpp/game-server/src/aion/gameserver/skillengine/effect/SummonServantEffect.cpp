#include "aion/gameserver/skillengine/effect/SummonServantEffect.h"

#include <cmath>
#include <cstdint>
#include <string>

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntention.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/geoEngine/collision/IgnoreProperties.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/NpcObjectType.h"
#include "aion/gameserver/model/gameobjects/Servant.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/properties/FirstTargetAttribute.h"
#include "aion/gameserver/skillengine/properties/Properties.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::skillengine::effect {

// Stored lambdas of the Java class (hub-headers.md §7.3): SummonServantEffect@L50 - the servant's delete, pinned on the servant and stored as
// its controller's DESPAWN task

namespace {

/** Java Math.toRadians(double) (JDK 9+: angdeg * DEGREES_TO_RADIANS) */
constexpr double toRadians(double angdeg) noexcept {
	return angdeg * 0.017453292519943295;
}

/** Seems to be around 2.5s (SummonServantEffect.java:INITIAL_SPAWN_DELAY) */
constexpr int64_t INITIAL_SPAWN_DELAY = 3000;

} // namespace

void SummonServantEffect::applyEffect(model::Effect& effect) const {
	gameserver::model::gameobjects::Creature& effector = *effect.getEffector();
	double radian = toRadians(utils::PositionUtil::convertHeadingToAngle(effect.getEffector()->getHeading()));
	float x = effector.getX() + static_cast<float>(std::cos(radian) * 2);
	float y = effector.getY() + static_cast<float>(std::sin(radian) * 2);
	geoEngine::math::Vector3f pos = world::geo::GeoService::getInstance().getClosestCollision(effector, x, y, effector.getZ(), true,
		getId(geoEngine::collision::CollisionIntention::DEFAULT_COLLISIONS), *geoEngine::collision::IgnoreProperties::of(effector.getRace()));
	runtime::Ref<gameserver::model::gameobjects::Servant> servant =
		spawnServant(effect, time, gameserver::model::gameobjects::NpcObjectType::SERVANT, pos.getX(), pos.getY(), pos.getZ());
	servant->getAi().onCreatureEvent(ai::event::AIEventType::ATTACK, *effect.getEffected());
}

runtime::Ref<gameserver::model::gameobjects::Servant> SummonServantEffect::spawnServant(model::Effect& effect, int32_t spawnDuration,
	gameserver::model::gameobjects::NpcObjectType npcObjectType, float x, float y, float z) const {
	gameserver::model::gameobjects::Creature& effector = *effect.getEffector();
	if (!effect.getEffected()) {
		const properties::Properties* properties = effect.getSkillTemplate()->getProperties();
		if (properties == nullptr)
			throw runtime::NullPointerException("SkillTemplate.getProperties() is null"); // Java: getProperties().getFirstTarget()
		if (properties->getFirstTarget() != properties::FirstTargetAttribute::POINT)
			throw runtime::IllegalArgumentException("Servant " + std::to_string(npcId) + "cannot be spawned by " + effector.toString() + " (target: null)");
	}

	runtime::Ref<gameserver::model::templates::spawns::SpawnTemplate> spawn =
		spawnengine::SpawnEngine::newSingleTimeSpawn(effector.getWorldId(), npcId, x, y, z, effector.getHeading());
	runtime::Ref<gameserver::model::gameobjects::Servant> servant =
		spawnengine::VisibleObjectSpawner::spawnServant(*spawn, effector.getInstanceId(), effector, effect.getSkillLevel(), npcObjectType);

	gameserver::model::gameobjects::Servant& spawned = *servant;
	runtime::FutureRef task = utils::ThreadPoolManager::getInstance().schedule(
		{&spawned}, [&spawned] { spawned.getController().delete_(); }, spawnDuration * int64_t{1000} + INITIAL_SPAWN_DELAY);
	spawned.getController().addTask(gameserver::model::TaskId::DESPAWN, std::move(task));
	return servant;
}

} // namespace aion::gameserver::skillengine::effect
