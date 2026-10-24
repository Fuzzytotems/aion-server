#include "aion/gameserver/handlers/ai/instance/rakes/EngineerLahulahuAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

AION_AI(EngineerLahulahuAI, "engineerlahulahu");

// Java EngineerLahulahuAI.java:39-53
void EngineerLahulahuAI::registerNpcs() {
	runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
	npc = runtime::Ref<Npc>(instance->getNpc(281111));
	npc1 = runtime::Ref<Npc>(instance->getNpc(281325));
	npc2 = runtime::Ref<Npc>(instance->getNpc(281323));
	npc3 = runtime::Ref<Npc>(instance->getNpc(281322));
	npc4 = runtime::Ref<Npc>(instance->getNpc(281326));
	npc5 = runtime::Ref<Npc>(instance->getNpc(281113));
	npc6 = runtime::Ref<Npc>(instance->getNpc(281324));
	npc7 = runtime::Ref<Npc>(instance->getNpc(281109));
	npc8 = runtime::Ref<Npc>(instance->getNpc(281112));
	npc9 = runtime::Ref<Npc>(instance->getNpc(281114));
	npc10 = runtime::Ref<Npc>(instance->getNpc(281108));
	npc11 = runtime::Ref<Npc>(instance->getNpc(281110));
}

// Java EngineerLahulahuAI.java:55-59
void EngineerLahulahuAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java EngineerLahulahuAI.java:61-74
void EngineerLahulahuAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 95:
			registerNpcs();
			AIActions::useSkill(*this, 18131);
			useSkills();
			break;
		case 25:
			getEffectController()->removeEffect(18131);
			AIActions::useSkill(*this, 18132);
			break;
		default:
			break;
	}
}

// Java EngineerLahulahuAI.java:76-80
void EngineerLahulahuAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
}

// Java EngineerLahulahuAI.java:82-84
void EngineerLahulahuAI::doSchedule() {
	ThreadPoolManager::getInstance().schedule(this, &EngineerLahulahuAI::useSkills, 10000);
}

// Java EngineerLahulahuAI.java:86-157
void EngineerLahulahuAI::useSkills() {
	if (getPosition()->isSpawned() && !isDead() && hpPhases->getCurrentPhase() > 0) {
		int32_t rnd = commons::utils::Rnd::get(1, 8);
		switch (rnd) {
			case 1:
				castOn(npc);
				castOn(npc1);
				break;
			case 2:
				castOn(npc2);
				castOn(npc3);
				break;
			case 3:
				castOn(npc4);
				castOn(npc5);
				break;
			case 4:
				castOn(npc6);
				castOn(npc7);
				break;
			case 5:
				castOn(npc8);
				break;
			case 6:
				castOn(npc9);
				break;
			case 7:
				castOn(npc10);
				break;
			case 8:
				castOn(npc11);
				break;
			default:
				break;
		}
		doSchedule();
	}
}

// Java EngineerLahulahuAI.java:91-94 and the eleven blocks like it: `if (npcN != null) { npcN.setTarget(npcN); npcN.getController().useSkill(skill); }`
void EngineerLahulahuAI::castOn(runtime::Field<runtime::Ref<Npc>>& device) {
	runtime::Ref<Npc> deviceNpc = device.get();
	if (deviceNpc != nullptr) {
		deviceNpc->setTarget(runtime::Ptr<VisibleObject>(*deviceNpc));
		deviceNpc->getController().useSkill(skill.get());
	}
}

} // namespace aion::gameserver::handlers::ai::instance::rakes
