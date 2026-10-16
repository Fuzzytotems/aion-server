#include "aion/gameserver/handlers/ai/instance/darkPoeta/MarabataControllerAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

AION_AI(MarabataControllerAI, "marabata_controller");

// Java MarabataControllerAI.java:20-28
runtime::Ptr<Npc> MarabataControllerAI::getBoss() {
	runtime::Ptr<Npc> npc;
	switch (getNpcId()) {
		case 700443:
		case 700444:
		case 700442:
			npc = getPosition()->getWorldMapInstance()->getNpc(214850);
			break;
		case 700446:
		case 700447:
		case 700445:
			npc = getPosition()->getWorldMapInstance()->getNpc(214851);
			break;
		case 700440:
		case 700441:
		case 700439:
			npc = getPosition()->getWorldMapInstance()->getNpc(214849);
			break;
		default:
			npc = nullptr;
			break;
	}
	return npc;
}

// Java MarabataControllerAI.java:30-57
void MarabataControllerAI::applyEffect(bool remove) {
	runtime::Ptr<Npc> boss = getBoss();
	if (boss != nullptr && !boss->isDead()) {
		switch (getNpcId()) {
			case 700443:
			case 700446:
			case 700440:
				if (remove)
					boss->getEffectController()->removeEffect(18556);
				else
					boss->getController().useSkill(18556);
				break;
			case 700444:
			case 700447:
			case 700441:
				// TODO unk
				break;
			case 700442:
			case 700445:
			case 700439:
				if (remove)
					boss->getEffectController()->removeEffect(18110);
				else
					boss->getController().useSkill(18110);
				break;
		}
	}
}

// Java MarabataControllerAI.java:59-64
void MarabataControllerAI::handleDied() {
	NpcAI::handleDied();
	applyEffect(true);
	AIActions::deleteOwner(*this);
}

// Java MarabataControllerAI.java:66-71
void MarabataControllerAI::handleSpawned() {
	NpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { useSkill(); }, 3500);
	ThreadPoolManager::getInstance().schedule({this}, [this] { applyEffect(false); }, 10000);
}

// Java MarabataControllerAI.java:73-85
void MarabataControllerAI::useSkill() {
	if (isDead())
		return;
	AIActions::targetSelf(*this);
	int32_t skill;
	switch (getNpcId()) {
		case 700443:
		case 700446:
		case 700440:
			skill = 18554;
			break;
		case 700444:
		case 700447:
		case 700441:
			skill = 18555;
			break;
		case 700442:
		case 700445:
		case 700439:
			skill = 18553;
			break;
		default:
			skill = 0;
			break;
	}
	AIActions::useSkill(*this, skill);
}

// Java MarabataControllerAI.java:87-93
bool MarabataControllerAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
