#include "aion/gameserver/skillengine/effect/SummonGroupGateEffect.h"

#include <cstdint>
#include <utility>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/GroupGate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::skillengine::effect {

// Stored lambdas of the Java class (hub-headers.md §7.3): SummonGroupGateEffect$1 (SummonGroupGateEffect_Runnable) - the group gate's delete
// after the effect's time, pinned on the gate (the fieldmap's captured `const Ref<GroupGate> groupgate`) and stored as its DESPAWN task

// Java SummonGroupGateEffect.java:27-47
void SummonGroupGateEffect::applyEffect(model::Effect& effect) const {
	gameserver::model::gameobjects::Creature& effector = *effect.getEffector();
	float x = effect.getX();
	float y = effect.getY();
	float z = effect.getZ();
	int8_t heading = effector.getHeading();
	int32_t worldId = effector.getWorldId();
	int32_t instanceId = effector.getInstanceId();
	runtime::Ref<gameserver::model::templates::spawns::SpawnTemplate> spawn =
		spawnengine::SpawnEngine::newSingleTimeSpawn(worldId, npcId, x, y, z, heading);
	runtime::Ref<gameserver::model::gameobjects::GroupGate> groupgate = spawnengine::VisibleObjectSpawner::spawnGroupGate(*spawn, instanceId, effector);
	gameserver::model::gameobjects::GroupGate& gate = *groupgate;
	runtime::FutureRef task = utils::ThreadPoolManager::getInstance().schedule({&gate}, [&gate] { gate.getController().delete_(); }, time * 1000);
	gate.getController().addTask(gameserver::model::TaskId::DESPAWN, std::move(task));
}

} // namespace aion::gameserver::skillengine::effect
