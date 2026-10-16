#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/SuramaTheTraitorAI.h"

#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(SuramaTheTraitorAI, "suramathetraitor");

// Java SuramaTheTraitorAI.java:26-30
void SuramaTheTraitorAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	moveToRaksha();
}

// Java SuramaTheTraitorAI.java:32-36
void SuramaTheTraitorAI::handleDied() {
	GeneralNpcAI::handleDied();
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 390845, 2000);
}

// Java SuramaTheTraitorAI.java:38-44
void SuramaTheTraitorAI::moveToRaksha() {
	setStateIfNot(AIState::WALKING);
	getOwner().setState(CreatureState::ACTIVE, true);
	getMoveController()->moveToPoint(651, 1319, 487);
	PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getOwner().getObjectId()));
	ThreadPoolManager::getInstance().schedule(this, &SuramaTheTraitorAI::startDialog, 10000);
}

// Java SuramaTheTraitorAI.java:46-56 (the lambda pins this and Raksha; no Raksha: Java's NullPointerException)
void SuramaTheTraitorAI::startDialog() {
	runtime::Ptr<Npc> raksha = getPosition()->getWorldMapInstance()->getNpc(219356);
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 390841);
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 390842, 3000);
	PacketSendUtility::broadcastMessage(raksha, 390843, 6000);
	Npc& pinnedRaksha = *raksha;
	ThreadPoolManager::getInstance().schedule({this, &pinnedRaksha}, [this, &pinnedRaksha] {
		pinnedRaksha.setTarget(runtime::Ptr<VisibleObject>(getOwner()));
		SkillEngine::getInstance().getSkill(pinnedRaksha, 20952, 60, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
		pinnedRaksha.overrideNpcType(CreatureType::ATTACKABLE);
	}, 8000);
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
