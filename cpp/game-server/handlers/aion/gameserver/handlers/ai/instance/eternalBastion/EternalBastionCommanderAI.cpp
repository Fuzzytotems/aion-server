#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionCommanderAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

AION_AI(EternalBastionCommanderAI, "eternal_bastion_commander");

// Java EternalBastionCommanderAI.java:28-38
void EternalBastionCommanderAI::handleFinishAttack() {
	if (!canThink())
		return;
	gameserver::ai::manager::EmoteManager::emoteStopAttacking(getOwner());
	getOwner().getController().loseAggro(false);
	hpRestoreTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		getOwner().getLifeStats()->triggerRestoreTask();
		wasAttacked = false;
	}, 120 * 1000));
}

// Java EternalBastionCommanderAI.java:40-43
void EternalBastionCommanderAI::cancelTask() {
	runtime::FutureRef task = hpRestoreTask.get();
	if (task != nullptr && !task->isDone())
		task->cancel(false);
}

// Java EternalBastionCommanderAI.java:45-55
void EternalBastionCommanderAI::handleCreatureAggro(Creature& creature) {
	if (runtime::Ptr<Npc> npc = runtime::as<Npc>(runtime::Ptr<Creature>(creature))) {
		if (npc->getNpcId() == 231130) { // Pashid
			getAggroList().addHate(*npc, 10000000); // Retail
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500768 + model::getRaceId(getRace()) * 4);
		}
	}
	cancelTask();
	EternalBastionAggressiveNpcAI::handleCreatureAggro(creature);
}

// Java EternalBastionCommanderAI.java:57-64
void EternalBastionCommanderAI::handleAttack(runtime::Ptr<Creature> creature) {
	EternalBastionAggressiveNpcAI::handleAttack(creature);
	cancelTask();
	if (!wasAttacked.get() || commons::utils::Rnd::chance() < 1)
		PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5b_TD_Notice_05());
	wasAttacked = true;
}

// Java EternalBastionCommanderAI.java:66-71
void EternalBastionCommanderAI::handleBackHome() {
	EternalBastionAggressiveNpcAI::handleBackHome();
	int32_t msgId = getNpcId() == 209516 ? 1500763 : 1500767;
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), msgId);
}

// Java EternalBastionCommanderAI.java:73-77
void EternalBastionCommanderAI::handleDespawned() {
	EternalBastionAggressiveNpcAI::handleDespawned();
	cancelTask();
}

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
