#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionConstructAI.h"

#include "aion/commons/utils/TimeUtils.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

AION_AI(EternalBastionConstructAI, "eternal_bastion_construct");

// Java EternalBastionConstructAI.java:23-26
bool EternalBastionConstructAI::canThink() {
	return false;
}

// Java EternalBastionConstructAI.java:28-36
void EternalBastionConstructAI::handleAttack(runtime::Ptr<Creature> creature) {
	if (getNpcId() == 831333 || getNpcId() == 831335) {
		if (commons::utils::currentTimeMillis() - lastMsgTime.get() > 30000) {
			lastMsgTime = commons::utils::currentTimeMillis();
			broadcastMsg(getNpcId() == 831333 ? SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5b_TD_Notice_03() : SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5b_TD_Notice_01());
		}
	}
}

// Java EternalBastionConstructAI.java:38-40
void EternalBastionConstructAI::broadcastMsg(SM_SYSTEM_MESSAGE msg) {
	PacketSendUtility::broadcastToMap(getOwner(), msg);
}

// Java EternalBastionConstructAI.java:42-49
bool EternalBastionConstructAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return true;
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
