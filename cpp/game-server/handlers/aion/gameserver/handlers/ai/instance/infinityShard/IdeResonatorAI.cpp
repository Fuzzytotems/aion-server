#include "aion/gameserver/handlers/ai/instance/infinityShard/IdeResonatorAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::infinityShard {

AION_AI(IdeResonatorAI, "ide_resonator");

// Java IdeResonatorAI.java:26-58. AIActions.targetCreature(this, null) when Hyperion is gone clears the owner's target. The lambdas pin this
// and Hyperion while he is there (Java's capture; the null checks stay).
void IdeResonatorAI::handleSpawned() {
	NpcAI::handleSpawned();
	runtime::Ptr<Npc> hyperion = getPosition()->getWorldMapInstance()->getNpc(231073);
	if (hyperion != nullptr)
		AIActions::targetCreature(*this, *hyperion);
	else
		getOwner().setTarget(nullptr);
	runtime::Ref<Npc> pinnedHyperion(hyperion);
	ThreadPoolManager::getInstance().schedule({this}, [this, pinnedHyperion] {
		if (!isDead() && pinnedHyperion != nullptr && !pinnedHyperion->isDead()) {
			int32_t firstBuff;
			switch (getNpcId()) {
				case 231093:
					firstBuff = 21381;
					break;
				case 231094:
					firstBuff = 21383;
					break;
				default:
					firstBuff = 21257; // NCSoft only implemented 3 different skill IDs for those temporary buffs
					break;
			}
			AIActions::useSkill(*this, firstBuff);
		}
	}, 8000);

	task.set(ThreadPoolManager::getInstance().schedule({this}, [this, pinnedHyperion] {
		if (!isDead() && pinnedHyperion != nullptr && !pinnedHyperion->isDead()) {
			int32_t secondBuff = 0;
			if (!pinnedHyperion->getEffectController()->hasAbnormalEffect(21258))
				secondBuff = 21258;
			else if (!pinnedHyperion->getEffectController()->hasAbnormalEffect(21382))
				secondBuff = 21382;
			else if (!pinnedHyperion->getEffectController()->hasAbnormalEffect(21384))
				secondBuff = 21384;
			else if (!pinnedHyperion->getEffectController()->hasAbnormalEffect(21416))
				secondBuff = 21416;

			if (secondBuff != 0)
				AIActions::useSkill(*this, secondBuff);
		}
	}, 18000));
}

// Java IdeResonatorAI.java:60-65
void IdeResonatorAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t /*skillLevel*/) {
	switch (skillTemplate->getSkillId()) {
		case 21258:
		case 21382:
		case 21384:
		case 21416:
			SkillEngine::getInstance().applyEffectDirectly(21371, getOwner(), getOwner());
			break;
		default:
			break;
	}
}

// Java IdeResonatorAI.java:67-70
void IdeResonatorAI::cancelTask() {
	runtime::FutureRef current = task.get();
	if (current != nullptr && !current->isCancelled())
		current->cancel(true);
}

// Java IdeResonatorAI.java:72-76
void IdeResonatorAI::handleDied() {
	cancelTask();
	NpcAI::handleDied();
}

// Java IdeResonatorAI.java:78-82
void IdeResonatorAI::handleDespawned() {
	cancelTask();
	NpcAI::handleDespawned();
}

// Java IdeResonatorAI.java:84-90
bool IdeResonatorAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::infinityShard
