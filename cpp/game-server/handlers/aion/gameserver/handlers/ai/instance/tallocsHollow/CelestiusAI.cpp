#include "aion/gameserver/handlers/ai/instance/tallocsHollow/CelestiusAI.h"

#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::tallocsHollow {

AION_AI(CelestiusAI, "celestius");

// Java CelestiusAI.java:36-41
void CelestiusAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if ((runtime::as<Player>(creature) != nullptr || runtime::as<Summon>(creature) != nullptr) && isSpawnTaskStarted.compareAndSet(false, true))
		startHelpersCall();
}

// Java CelestiusAI.java:43-52 (Java casts each spawn to Npc)
void CelestiusAI::startHelpersCall() {
	helpersTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (!isDead()) {
			SkillEngine::getInstance().getSkill(getOwner(), 18981, 44, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
			startWalker(*runtime::as<Npc>(spawn(SUMMONS_ID, 518.0f, 813.0f, 1378.0f, int8_t{0})), "3001900001");
			startWalker(*runtime::as<Npc>(spawn(SUMMONS_ID, 551.0f, 795.0f, 1376.0f, int8_t{0})), "3001900002");
			startWalker(*runtime::as<Npc>(spawn(SUMMONS_ID, 574.0f, 854.0f, 1375.0f, int8_t{0})), "3001900003");
		}
	}, 1000, 25000));
}

// Java CelestiusAI.java:54-59
void CelestiusAI::startWalker(Npc& npc, std::string_view walkId) {
	npc.getSpawn()->setWalkerId(walkId);
	WalkManager::startWalking(dynamic_cast<NpcAI&>(npc.getAi()));
	npc.setState(CreatureState::ACTIVE, true);
	PacketSendUtility::broadcastPacket(npc, SM_EMOTION(npc, EmotionType::CHANGE_SPEED, 0, npc.getObjectId()));
}

// Java CelestiusAI.java:61-64
void CelestiusAI::cleanUp() {
	cancelTask();
	deleteSummons();
}

// Java CelestiusAI.java:66-70
void CelestiusAI::cancelTask() {
	runtime::FutureRef task = helpersTask.get();
	if (task != nullptr && !task->isDone()) {
		task->cancel(true);
	}
}

// Java CelestiusAI.java:72-74
void CelestiusAI::deleteSummons() {
	for (const runtime::Ptr<Npc>& npc : getPosition()->getWorldMapInstance()->getNpcs({SUMMONS_ID}))
		npc->getController().delete_();
}

// Java CelestiusAI.java:76-80
void CelestiusAI::handleBackHome() {
	cleanUp();
	AggressiveNpcAI::handleBackHome();
}

// Java CelestiusAI.java:82-86
void CelestiusAI::handleDespawned() {
	cleanUp();
	AggressiveNpcAI::handleDespawned();
}

// Java CelestiusAI.java:88-92
void CelestiusAI::handleDied() {
	cleanUp();
	AggressiveNpcAI::handleDied();
}

} // namespace aion::gameserver::handlers::ai::instance::tallocsHollow
