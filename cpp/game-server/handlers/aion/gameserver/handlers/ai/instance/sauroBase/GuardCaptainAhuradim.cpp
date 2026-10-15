#include "aion/gameserver/handlers/ai/instance/sauroBase/GuardCaptainAhuradim.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::sauroBase {

AION_AI(GuardCaptainAhuradim, "guard_captain_ahuradim");

// Java GuardCaptainAhuradim.java:31-35
void GuardCaptainAhuradim::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java GuardCaptainAhuradim.java:37-49
void GuardCaptainAhuradim::handleHpPhase(int32_t phaseHpPercent) {
	if (phaseHpPercent == 100) {
		startGeneratorTask();
	} else {
		getOwner().queueSkill(21194, 1, 0, NpcSkillTargetAttribute::ME);
		if (commons::utils::Rnd::nextBoolean()) {
			getOwner().queueSkill(21429, 1, 5000);
		} else {
			getOwner().queueSkill(21190, 1, 5000, NpcSkillTargetAttribute::ME);
		}
	}
}

// Java GuardCaptainAhuradim.java:51-66 (the inner lambda keeps the generator alive, as Java's capture does)
void GuardCaptainAhuradim::startGeneratorTask() {
	scheduledTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (hpPhases->getCurrentPhase() > 0 && !getOwner().getEffectController()->hasAbnormalEffect(21191)) {
			for (const runtime::Ptr<Npc>& found : getPosition()->getWorldMapInstance()->getNpcs({284437})) {
				runtime::Ref<Npc> generator(*found);
				generator->setTarget(runtime::Ptr<VisibleObject>(getOwner()));
				PacketSendUtility::broadcastMessage(generator, 1501014);
				scheduledTask.set(ThreadPoolManager::getInstance().schedule({this}, [this, generator] {
					if (hpPhases->getCurrentPhase() > 0 && !generator->isDead()) {
						PacketSendUtility::broadcastMessage(generator, 1501015);
						SkillEngine::getInstance().getSkill(*generator, 21200, 1, generator)->useWithoutPropSkill();
					}
				}, 15 * 1000));
			}
		}
	}, 40 * 1000));
}

// Java GuardCaptainAhuradim.java:68-75
void GuardCaptainAhuradim::onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	switch (skillTemplate->getSkillId()) {
		case 20775:
			PacketSendUtility::broadcastMessage(getOwner(), 1500783); // Weaklings! I will destroy you myself!
			break;
		case 21190:
		case 21429:
			PacketSendUtility::broadcastMessage(getOwner(), 1500785); // I'm Achradim, the Steel Wall Protector!
			break;
		case 21194:
			PacketSendUtility::broadcastMessage(getOwner(), 1500786); // You're very persistent. Allow me to demonstrate my power!
			break;
		default:
			break;
	}
}

// Java GuardCaptainAhuradim.java:77-81
void GuardCaptainAhuradim::cancelTasks() {
	runtime::FutureRef task = scheduledTask.get();
	if (task != nullptr) {
		task->cancel(false);
	}
}

// Java GuardCaptainAhuradim.java:83-88
void GuardCaptainAhuradim::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
	cancelTasks();
}

// Java GuardCaptainAhuradim.java:90-94
void GuardCaptainAhuradim::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelTasks();
}

} // namespace aion::gameserver::handlers::ai::instance::sauroBase
