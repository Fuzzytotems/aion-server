#include "aion/gameserver/skillengine/effect/SummonEffect.h"

#include "aion/gameserver/controllers/SummonController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/summons/UnsummonType.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/summons/SummonsService.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::skillengine::effect {

// Stored lambdas of the Java class (hub-headers.md §7.3): SummonEffect@L28 - the live time's release, pinned on the summon and stored as its
// controller's DESPAWN task

void SummonEffect::applyEffect(model::Effect& effect) const {
	gameserver::model::gameobjects::player::Player& effected = *runtime::cast<gameserver::model::gameobjects::player::Player>(effect.getEffected());
	runtime::Ptr<gameserver::model::gameobjects::Summon> summon =
		services::summons::SummonsService::createSummon(effected, npcId, effect.getSkillId(), effect.getSkillLevel(), time);
	if (summon && time > 0) {
		gameserver::model::gameobjects::Summon& spawned = *summon;
		runtime::FutureRef task = utils::ThreadPoolManager::getInstance().schedule(
			{&spawned}, [&spawned] { spawned.getController().release(gameserver::model::summons::UnsummonType::UNSPECIFIED); }, time * 1000);
		spawned.getController().addTask(gameserver::model::TaskId::DESPAWN, std::move(task));
		effected.getEffectController()->removePetOrderUnSummonEffects();
	}
}

void SummonEffect::calculate(model::Effect& effect) const {
	effect.addSuccessEffect(this);
}

} // namespace aion::gameserver::skillengine::effect
