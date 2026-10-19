#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/DynatoumMaintenanceDeviceAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

AION_AI(DynatoumMaintenanceDeviceAI, "dynatoum_maintenance_device");

// Java DynatoumMaintenanceDeviceAI.java:25-28
bool DynatoumMaintenanceDeviceAI::canThink() {
	return false;
}

// Java DynatoumMaintenanceDeviceAI.java:30-36 (a missing Dynatoum is Java's NullPointerException, as the dereference)
void DynatoumMaintenanceDeviceAI::handleSpawned() {
	NpcAI::handleSpawned();
	runtime::Ptr<Npc> dynatoum = getPosition()->getWorldMapInstance()->getNpc(getPosition()->getMapId() == 301230000 ? 233740 : 234686);
	AIActions::targetCreature(*this, *dynatoum);
	scheduleSkillTask();
}

// Java DynatoumMaintenanceDeviceAI.java:38-43
void DynatoumMaintenanceDeviceAI::scheduleSkillTask() {
	skillTask = ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead())
			AIActions::useSkill(*this, 21535);
	}, 10000);
}

// Java DynatoumMaintenanceDeviceAI.java:45-49
void DynatoumMaintenanceDeviceAI::onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate == nullptr) // Java: skillTemplate.getSkillId() on null
		throw runtime::NullPointerException("DynatoumMaintenanceDeviceAI: skill template");
	if (skillTemplate->getSkillId() == 21535)
		scheduleSkillTask();
}

// Java DynatoumMaintenanceDeviceAI.java:51-55 (a null task is Java's NullPointerException, as the dereference)
void DynatoumMaintenanceDeviceAI::handleDespawned() {
	skillTask.get()->cancel(true);
	NpcAI::handleDespawned();
}

// Java DynatoumMaintenanceDeviceAI.java:57-63
bool DynatoumMaintenanceDeviceAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
