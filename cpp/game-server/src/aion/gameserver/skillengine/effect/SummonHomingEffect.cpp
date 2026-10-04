#include "aion/gameserver/skillengine/effect/SummonHomingEffect.h"

#include <cstdint>

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Homing.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::skillengine::effect {

using runtime::Ref;

/**
 * Java: the anonymous ActionObserver(ObserverType.ATTACK) of SummonHomingEffect.applyEffect (SummonHomingEffect.java:49-58, fieldmap key
 * SummonHomingEffect$1): each attack of the homing counts one off its attacks, and the last one deletes it. It captures the homing. Stored in the
 * homing's ObserveController and the effect's observerRemoveTasks (Effect.addObserver).
 */
struct SummonHomingEffect_ActionObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	const Ref<gameserver::model::gameobjects::Homing> homing; // captured local Homing homing

	static Ref<SummonHomingEffect_ActionObserver> create(gameserver::model::gameobjects::Homing& homing) {
		return runtime::makeRef<SummonHomingEffect_ActionObserver>(homing);
	}

	void attack(gameserver::model::gameobjects::Creature& /*creature*/, int32_t /*skillId*/) override {
		homing->setAttackCount(homing->getAttackCount() - 1);
		if (homing->getAttackCount() <= 0)
			homing->getController().delete_();
	}

protected:
	explicit SummonHomingEffect_ActionObserver(gameserver::model::gameobjects::Homing& homingValue)
		: ActionObserver(controllers::observer::ObserverType::ATTACK), homing(homingValue) {}
	~SummonHomingEffect_ActionObserver() override = default;
};

// Anonymous class com.aionemu.gameserver.skillengine.effect.SummonHomingEffect$1: the struct above. Stored lambda SummonHomingEffect@L60: the
// homing's delete, pinned on the homing and stored as its controller's DESPAWN task.
void SummonHomingEffect::applyEffect(model::Effect& effect) const {
	gameserver::model::gameobjects::Creature& effector = *effect.getEffector();
	float x = effector.getX();
	float y = effector.getY();
	float z = effector.getZ();
	int8_t heading = effector.getHeading();
	int32_t worldId = effector.getWorldId();
	int32_t instanceId = effector.getInstanceId();

	for (int32_t i = 0; i < npcCount; i++) {
		runtime::Ref<gameserver::model::templates::spawns::SpawnTemplate> spawn =
			spawnengine::SpawnEngine::newSingleTimeSpawn(worldId, npcId, x, y, z, heading);
		const runtime::Ref<gameserver::model::gameobjects::Homing> homing =
			spawnengine::VisibleObjectSpawner::spawnHoming(*spawn, instanceId, effector, attackCount, effect.getSkillId());

		if (attackCount > 0) {
			effect.addObserver(*homing, *SummonHomingEffect_ActionObserver::create(*homing));
		}
		// Schedule a despawn just in case
		gameserver::model::gameobjects::Homing& spawned = *homing;
		runtime::FutureRef task =
			utils::ThreadPoolManager::getInstance().schedule({&spawned}, [&spawned] { spawned.getController().delete_(); }, 15 * 1000);
		spawned.getController().addTask(gameserver::model::TaskId::DESPAWN, std::move(task));
		spawned.getAi().onCreatureEvent(ai::event::AIEventType::ATTACK, *effect.getEffected());
	}
}

} // namespace aion::gameserver::skillengine::effect
