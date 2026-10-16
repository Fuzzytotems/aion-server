#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/PazuzuAI.h"

#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

AION_AI(PazuzuAI, "pazuzu");

// Java PazuzuAI.java:29-36
void PazuzuAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isHome.compareAndSet(true, false)) {
		PacketSendUtility::broadcastMessage(getOwner(), 342219);
		startTask();
	}
}

// Java PazuzuAI.java:38-43
void PazuzuAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	cancelTask();
	isHome.set(true);
}

// Java PazuzuAI.java:45-50
void PazuzuAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelTask();
	PacketSendUtility::broadcastMessage(getOwner(), 1500003);
}

// Java PazuzuAI.java:52-65 (Java gets both skills and never uses them: ported as is)
void PazuzuAI::startTask() {
	task.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		SkillEngine::getInstance().getSkill(getOwner(), 19145, 55, runtime::Ptr<VisibleObject>(getOwner()));
		if (getPosition()->getWorldMapInstance()->getNpc(281909) == nullptr) {
			runtime::Ptr<Npc> worms = runtime::cast<Npc>(spawn(281909, 651.351990f, 326.425995f, 465.523987f, int8_t{8}));
			spawn(281909, 666.604980f, 314.497009f, 465.394012f, int8_t{27});
			spawn(281909, 685.588989f, 342.955994f, 465.908997f, int8_t{68});
			spawn(281909, 651.322021f, 346.554993f, 465.563995f, int8_t{111});
			spawn(281909, 666.7373f, 314.2235f, 465.38953f, int8_t{30});

			SkillEngine::getInstance().getSkill(*worms, 19291, 55, runtime::Ptr<VisibleObject>(getOwner()));
		}
	}, 0, 70000));
}

// Java PazuzuAI.java:67-71
void PazuzuAI::cancelTask() {
	runtime::FutureRef current = task.get();
	if (current != nullptr && !current->isCancelled()) {
		current->cancel(true);
	}
}

// Java PazuzuAI.java:73-77
void PazuzuAI::handleDespawned() {
	cancelTask();
	AggressiveNpcAI::handleDespawned();
}

// Java PazuzuAI.java:79-85
bool PazuzuAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter
