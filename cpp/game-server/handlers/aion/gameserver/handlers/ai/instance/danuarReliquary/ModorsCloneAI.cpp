#include "aion/gameserver/handlers/ai/instance/danuarReliquary/ModorsCloneAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h" // the Ref<Effect> applyEffectDirectly returns
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::danuarReliquary {

AION_AI(ModorsCloneAI, "modors_clone");

// Java ModorsCloneAI.java:26-35
void ModorsCloneAI::handleSpawned() {
	AggressiveNoLootNpcAI::handleSpawned();
	if (getNpcId() == 284383 || getNpcId() == 855244) {
		ThreadPoolManager::getInstance().schedule({this}, [this] { PacketSendUtility::broadcastMessage(getOwner(), 348682); }, 1000);
	} else {
		SkillEngine::getInstance().applyEffectDirectly(21181, getOwner(), getOwner()); // Malevolence
		ThreadPoolManager::getInstance().schedule({this}, [this] { PacketSendUtility::broadcastMessage(getOwner(), 348683); }, 1000);
	}
}

// Java ModorsCloneAI.java:37-40
float ModorsCloneAI::modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) {
	return damage * modifier.get();
}

// Java ModorsCloneAI.java:42-75 (the fakes' loop captures the skill id and level, not the template pointer)
void ModorsCloneAI::onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	switch (skillTemplate->getSkillId()) {
		case 21177:
			if (getNpcId() == 284383 || getNpcId() == 855244) {
				PacketSendUtility::broadcastMessage(getOwner(), 348677);
				spawn(284387, 255.98627f, 259.0136f, 242.73842f, int8_t{0});
			} else {
				PacketSendUtility::broadcastMessage(getOwner(), 348680);
			}
			break;
		case 21372:
		case 21174:
			if (getNpcId() == 284383 || getNpcId() == 855244) {
				PacketSendUtility::broadcastMessage(getOwner(), 348678);
			} else {
				PacketSendUtility::broadcastMessage(getOwner(), 348679);
			}
			break;
		case 21175:
			break;
		default:
			break;
	}
	if (getNpcId() == 284383 || getNpcId() == 855244) {
		const int32_t skillId = skillTemplate->getSkillId();
		getKnownList().forEachNpc([this, skillId, skillLevel](Npc& npc) {
			if (!npc.isDead() && (npc.getNpcId() == 284384 || npc.getNpcId() == 855245)) {
				runtime::Ptr<Creature> target = runtime::as<Creature>(npc.getTarget());
				if (target != nullptr && target->isDead()) {
					npc.setTarget(getTarget());
				}
				SkillEngine::getInstance().getSkill(npc, skillId, skillLevel, npc.getTarget())->useWithoutPropSkill();
			}
		});
	}
	modifier = 0.2f;
}

// Java ModorsCloneAI.java:77-80
void ModorsCloneAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	modifier = 1.35f;
}

// Java ModorsCloneAI.java:82-88
void ModorsCloneAI::cancelVengefulOrb() {
	runtime::Ptr<Npc> vengefulOrb = getPosition()->getWorldMapInstance()->getNpc(284387);
	if (vengefulOrb != nullptr) {
		vengefulOrb->getController().cancelCurrentSkill(nullptr);
		vengefulOrb->getController().delete_();
	}
}

// Java ModorsCloneAI.java:90-93
int32_t ModorsCloneAI::modifyInitialSkillDelay(int32_t delay) {
	return 3000;
}

// Java ModorsCloneAI.java:95-99
void ModorsCloneAI::handleDied() {
	cancelVengefulOrb();
	AggressiveNoLootNpcAI::handleDied();
}

// Java ModorsCloneAI.java:101-105
void ModorsCloneAI::handleDespawned() {
	cancelVengefulOrb();
	AggressiveNoLootNpcAI::handleDespawned();
}

} // namespace aion::gameserver::handlers::ai::instance::danuarReliquary
