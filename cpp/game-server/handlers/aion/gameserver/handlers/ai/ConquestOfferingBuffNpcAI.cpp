#include "aion/gameserver/handlers/ai/ConquestOfferingBuffNpcAI.h"

#include <string>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai {

AION_AI(ConquestOfferingBuffNpcAI, "conquest_offering_buff_npc");

// Java ConquestOfferingBuffNpcAI.java:27-32. The despawn task is pinned on this AI (a part of its npc).
void ConquestOfferingBuffNpcAI::handleSpawned() {
	ActionItemNpcAI::handleSpawned();
	sendWakeUpMsg();
	despawnTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().getController().delete_(); }, 65000));
}

// Java ConquestOfferingBuffNpcAI.java:34-42
void ConquestOfferingBuffNpcAI::handleUseItemFinish(Player& player) {
	if (used.compareAndSet(false, true)) {
		sendTalkedMsg();
		int32_t skillId = 21924 + commons::utils::Rnd::get(0, 3);
		runtime::Ref<skillengine::model::Skill> skill = SkillEngine::getInstance().getSkill(getOwner(), skillId, 1, player);
		if (skill == nullptr) // Java: useSkill() of a null skill (no template)
			throw runtime::NullPointerException("skill " + std::to_string(skillId) + " has no template");
		skill->useSkill();
		getOwner().getController().delete_();
	}
}

// Java ConquestOfferingBuffNpcAI.java:44-48
void ConquestOfferingBuffNpcAI::handleDied() {
	ActionItemNpcAI::handleDied();
	cancelTask();
}

// Java ConquestOfferingBuffNpcAI.java:50-54
void ConquestOfferingBuffNpcAI::handleDespawned() {
	cancelTask();
	ActionItemNpcAI::handleDespawned();
}

// Java ConquestOfferingBuffNpcAI.java:56-59
void ConquestOfferingBuffNpcAI::cancelTask() {
	runtime::FutureRef task = despawnTask.get();
	if (task != nullptr && !task->isDone())
		task->cancel(true);
}

// Java ConquestOfferingBuffNpcAI.java:61-64
void ConquestOfferingBuffNpcAI::sendWakeUpMsg() {
	int32_t msg = (1501279 + (commons::utils::Rnd::get(0, 2) * 2));
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), msg, 1500);
}

// Java ConquestOfferingBuffNpcAI.java:66-69
void ConquestOfferingBuffNpcAI::sendTalkedMsg() {
	int32_t msg = (1501280 + (commons::utils::Rnd::get(0, 2) * 2));
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), msg);
}

} // namespace aion::gameserver::handlers::ai
