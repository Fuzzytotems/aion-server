#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/EmpyreanAdministratorArminosAI.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

AION_AI(EmpyreanAdministratorArminosAI, "empadministratorarminos");

// Java EmpyreanAdministratorArminosAI.java:18-22
void EmpyreanAdministratorArminosAI::handleSpawned() {
	NpcAI::handleSpawned();
	startEvent();
}

// Java EmpyreanAdministratorArminosAI.java:24-46
void EmpyreanAdministratorArminosAI::startEvent() {
	switch (getNpcId()) {
		case 217744:
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500247, 8000);
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500250, 20000);
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500251, 60000);
			break;
		case 217749:
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500252, 8000);
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500253, 16000);
			PacketSendUtility::broadcastToMap(getOwner(), 1400982, 25000);
			PacketSendUtility::broadcastToMap(getOwner(), 1400988, 27000);
			PacketSendUtility::broadcastToMap(getOwner(), 1400989, 29000);
			PacketSendUtility::broadcastToMap(getOwner(), 1400990, 31000);
			PacketSendUtility::broadcastToMap(getOwner(), 1401013, 93000);
			PacketSendUtility::broadcastToMap(getOwner(), 1401014, 113000);
			PacketSendUtility::broadcastToMap(getOwner(), 1401015, 118000);
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500255, 118000);
			break;
			// case
			// despawn after 1min
		default:
			break;
	}
}

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
