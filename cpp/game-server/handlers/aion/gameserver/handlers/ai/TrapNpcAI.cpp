#include "aion/gameserver/handlers/ai/TrapNpcAI.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::handlers::ai {

AION_AI(TrapNpcAI, "trap");

// Java TrapNpcAI.java:31-35
void TrapNpcAI::handleCreatureSee(Creature& creature) {
	NpcAI::handleCreatureSee(creature);
	tryActivateTrap(creature);
}

// Java TrapNpcAI.java:37-41
void TrapNpcAI::handleCreatureMoved(Creature& creature) {
	NpcAI::handleCreatureMoved(creature);
	tryActivateTrap(creature);
}

// Java TrapNpcAI.java:43-58
void TrapNpcAI::tryActivateTrap(Creature& creature) {
	if (despawnTask.get()) {
		return;
	}

	if (!creature.isDead() && !creature.isInVisualState(CreatureVisualState::BLINKING)
		&& isInRange(creature, getOwner().getGameStats()->getAttackRange()->getCurrent())) {

		Creature& creator = *runtime::cast<Creature>(getCreator());
		if (!creator.isEnemy(creature)) {
			return;
		}
		explode(creature);
	}
}

// Java TrapNpcAI.java:60-69. Java's toLowerCase() of the template name: the names are ASCII.
void TrapNpcAI::handleSpawned() {
	std::string name = getObjectTemplate()->getName();
	std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (name != "scrapped mechanisms")
		getOwner().setVisualState(CreatureVisualState::HIDE1);
	NpcAI::handleSpawned();
	if (name == "shock trap")
		explode(getOwner());
}

// Java TrapNpcAI.java:71-83. The despawn lambda (fieldmap: pin {this}) is pinned on this AI, a part of its npc.
void TrapNpcAI::explode(Creature& creature) {
	if (setStateIfNot(AIState::FIGHT)) {
		getOwner().unsetVisualState(CreatureVisualState::HIDE1);
		PacketSendUtility::broadcastPacket(getOwner(), SM_PLAYER_STATE(getOwner()));
		AIActions::targetCreature(*this, creature);
		getOwner().updateKnownlist();
		runtime::Ptr<NpcSkillEntry> npcSkill = getSkillList()->getRandomSkill();
		if (npcSkill) {
			AIActions::useSkill(*this, npcSkill->getSkillId());
		}
		despawnTask = ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, 5000);
	}
}

// Java TrapNpcAI.java:85-88
bool TrapNpcAI::isMoveSupported() {
	return false;
}

// Java TrapNpcAI.java:90-95
bool TrapNpcAI::canHandleEvent(AIEventType eventType) {
	if (eventType == AIEventType::CREATURE_MOVED)
		return true;
	return NpcAI::canHandleEvent(eventType);
}

// Java TrapNpcAI.java:97-103
bool TrapNpcAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai
