#include "aion/gameserver/skillengine/effect/SummonFunctionalNpcEffect.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/SummonOwner.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::skillengine::effect {

// Stored lambdas of the Java class (hub-headers.md §7.3): SummonFunctionalNpcEffect$1 (SummonFunctionalNpcEffect_Runnable) - the functional
// npc's delete after 5 minutes, pinned on the npc (the fieldmap's captured `const Ref<Npc> functionalNpc`)

// Java SummonFunctionalNpcEffect.java:25-37
void SummonFunctionalNpcEffect::applyEffect(model::Effect& effect) const {
	gameserver::model::gameobjects::player::Player& effected = *runtime::cast<gameserver::model::gameobjects::player::Player>(effect.getEffected());
	// Java hands a null owner on (Npc.setSummonOwner(null); no skill row lacks one); VisibleObjectSpawner::spawnFunctionalNpc takes a SummonOwner
	// value, so a missing one throws here (P5-04.md, M5j stage 3 CP3)
	if (!owner)
		throw runtime::NullPointerException("SummonFunctionalNpcEffect.owner is null");
	runtime::Ref<gameserver::model::gameobjects::Npc> functionalNpc = spawnengine::VisibleObjectSpawner::spawnFunctionalNpc(effected, npcId, *owner);
	if (functionalNpc) { // Java: checked in the task
		gameserver::model::gameobjects::Npc& npc = *functionalNpc;
		utils::ThreadPoolManager::getInstance().schedule({&npc}, [&npc] {
			if (npc.isSpawned())
				npc.getController().delete_();
		}, 300000);
	}
}

} // namespace aion::gameserver::skillengine::effect
