#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/CalindiSummonsAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(CalindiSummonsAI, "calindi_summon");

// Java CalindiSummonsAI.java:26-40
void CalindiSummonsAI::handleSpawned() {
	NpcAI::handleSpawned();
	int32_t delay = getDelay();
	if (getFakeTextureNpcId() != 0) {
		textureObject.set(spawn(getFakeTextureNpcId(), getPosition()->getX(), getPosition()->getY(), getPosition()->getZ(), int8_t{0}));
	}
	task.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { AIActions::useSkill(*this, getSkillId()); }, 500, delay));
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		AIActions::deleteOwner(*this);
		if (runtime::Ptr<VisibleObject> texture = textureObject.get(); texture != nullptr) {
			texture->getController().delete_();
		}
	}, 15000);
}

// Java CalindiSummonsAI.java:42-55
int32_t CalindiSummonsAI::getSkillId() {
	switch (getNpcId()) {
		case 283131:
			return 20916;
		case 283133:
			return 20914;
		case 856298:
			return 21891;
		case 856299:
			return 21892;
		default:
			return 0;
	}
}

// Java CalindiSummonsAI.java:57-68
int32_t CalindiSummonsAI::getFakeTextureNpcId() {
	switch (getNpcId()) {
		case 283131:
		case 856299:
			return 283130;
		case 283133:
		case 856298:
			return 283132;
		default:
			return 0;
	}
}

// Java CalindiSummonsAI.java:70-81
int32_t CalindiSummonsAI::getDelay() {
	switch (getNpcId()) {
		case 283131:
		case 856299:
			return 2000;
		case 283133:
		case 856298:
			return 500;
		default:
			return 0;
	}
}

// Java CalindiSummonsAI.java:83-87
void CalindiSummonsAI::handleDespawned() {
	task.get()->cancel(true);
	NpcAI::handleDespawned();
}

// Java CalindiSummonsAI.java:89-95
bool CalindiSummonsAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
