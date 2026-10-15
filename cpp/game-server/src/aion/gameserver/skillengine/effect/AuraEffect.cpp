#include "aion/gameserver/skillengine/effect/AuraEffect.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MANTRA_EFFECT.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;
using runtime::Ref;

/**
 * Java: the inner class AuraEffect.AuraTask (AuraEffect.java:79-96), scheduled at a fixed rate of 6,500 ms from 0 by startEffect and kept as a
 * periodic task of the effect. AuraEffect.h declares no nested AuraTask (the census counts its constructor and `run` as undeclared bodies), so the
 * class is this file-local task struct, mapped to its Java name for the concurrency lint (docs/deviations/P5-03.md, header request: a nested
 * `struct AuraTask;`, the approved m5b3-e-2 case of FearEffect::FearTask). A task object with only TaskArg members, ported as an aggregate
 * TaskStruct value (runtime-architecture.md §7.3, the FearEffect_FearTask precedent): the Ref retains the effect exactly as the inner class's
 * field does, and the captured this$0 is the immutable template. Effect.setPeriodicTask keeps the Future, and Effect.endEffect -> stopTasks
 * cancels it, which releases the task and its Ref (cycles.toml "AuraEffect.AuraTask.effect": java-hook).
 */
// fieldmap-class: com.aionemu.gameserver.skillengine.effect.AuraEffect.AuraTask
struct AuraEffect_AuraTask final : runtime::TaskStruct {
	const Ref<model::Effect> effect;
	const AuraEffect* auraEffect; // captured this (line 89), immortal static data

	void operator()() const {
		auraEffect->onPeriodicAction(*effect);
		/**
		 * This has the special effect of clearing the current thread's quantum and putting it to the end of the queue for its priority level. Will
		 * just give-up the thread's turn, and gain it in the next round.
		 */
		std::this_thread::yield();
	}
};

void AuraEffect::applyEffect(model::Effect& effect) const {
	Ptr<Creature> effector = effect.getEffector();
	if (runtime::as<Player>(effector) && effector->getEffectController()->findBySkillId(effect.getSkillId())) {
		utils::audit::AuditLogger::log(*runtime::cast<Player>(effector),
			"might be abusing CM_CASTSPELL mantra effect, skill id: " + std::to_string(effect.getSkillId()));
		return;
	}
	effect.addToEffectedController();
}

void AuraEffect::onPeriodicAction(model::Effect& effect) const {
	Ptr<Creature> effector = effect.getEffector();
	if (runtime::as<Npc>(effector)) {
		applyAuraTo(*effector);
	} else {
		Ptr<Player> p = runtime::cast<Player>(effector); // Java `(Player) effector`: ClassCastException for another creature
		if (!p->isOnline()) { // task check
			return;
		}
		if (p->isInTeam()) {
			int32_t rangeBoost = effector->getGameStats()->getStat(gameserver::model::stats::container::StatEnum::BOOST_MANTRA_RANGE, 100)->getCurrent();
			// Java `distanceZ * rangeBoost / 100f`: the int product (wrapping) divided as a float
			float rangeZ = static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(distanceZ) * static_cast<uint32_t>(rangeBoost))) / 100.0f;
			float range = static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(distance) * static_cast<uint32_t>(rangeBoost))) / 100.0f;
			for (Ptr<Player> player : p->getCurrentGroup()->getOnlineMembers()) {
				// Java `p.equals(player)`: AionObject.equals compares the objectIds, and a null argument is not equal
				if ((player && p->equals(*player))
					|| (std::abs(p->getZ() - player->getZ()) <= rangeZ && utils::PositionUtil::isInRange(*p, *player, range, false))) {
					applyAuraTo(*player);
				}
			}
		} else {
			applyAuraTo(*effector);
		}
	}
	utils::PacketSendUtility::broadcastPacket(*effector, network::aion::serverpackets::SM_MANTRA_EFFECT(*effector, skillId));
}

void AuraEffect::applyAuraTo(Creature& effected) const {
	SkillEngine::getInstance().applyEffect(skillId, effected, effected);
}

// Inner class com.aionemu.gameserver.skillengine.effect.AuraEffect.AuraTask: the task struct AuraEffect_AuraTask above
void AuraEffect::startEffect(model::Effect& effect) const {
	effect.setPeriodicTask(
		utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(AuraEffect_AuraTask{{}, Ref<model::Effect>(effect), this}, 0, 6500), position);
}

void AuraEffect::endEffect(model::Effect& /*effect*/) const {
}

} // namespace aion::gameserver::skillengine::effect
