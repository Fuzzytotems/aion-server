#include "aion/gameserver/handlers/ai/instance/kamarBf/KamarBossesAI.h"

#include "aion/gameserver/controllers/NpcController.h"

namespace aion::gameserver::handlers::ai::instance::kamarBf {

AION_AI(KamarBossesAI, "kamarbosses");

// Java KamarBossesAI.java:22-37
void KamarBossesAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	switch (getOwner().getNpcId()) {
		case 232853:
			PacketSendUtility::broadcastToMap(getOwner(), 1401845);
			break;
		case 232857:
			PacketSendUtility::broadcastToMap(getOwner(), 1401848);
			break;
		case 232858:
			PacketSendUtility::broadcastToMap(getOwner(), 1401850);
			break;
	}
}

// Java KamarBossesAI.java:39-56
void KamarBossesAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	switch (getOwner().getNpcId()) {
		case 232854:
		case 232853:
		case 232852:
			flag.set(runtime::cast<Npc>(spawn(801956, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0})));
			break;
		case 232857:
			flag.set(runtime::cast<Npc>(spawn(801957, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0})));
			break;
		case 232858:
			flag.set(runtime::cast<Npc>(spawn(801958, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0})));
			break;
	}
}

// Java KamarBossesAI.java:58-63
void KamarBossesAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	if (runtime::Ptr<Npc> f = flag.get(); f != nullptr)
		f->getController().delete_();
}

// Java KamarBossesAI.java:65-82
void KamarBossesAI::handleDied() {
	AggressiveNpcAI::handleDied();
	switch (getOwner().getNpcId()) {
		case 232853:
			PacketSendUtility::broadcastToMap(getOwner(), 1401846);
			break;
		case 232857:
			PacketSendUtility::broadcastToMap(getOwner(), 1401849);
			break;
		case 232858:
			PacketSendUtility::broadcastToMap(getOwner(), 1401851);
			break;
	}
	if (runtime::Ptr<Npc> f = flag.get(); f != nullptr)
		f->getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::kamarBf
