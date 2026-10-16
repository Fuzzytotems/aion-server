#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AdvanceCorridorShield.h"

#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/panesterra/ahserion/AhserionRaid.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

AION_AI(AdvanceCorridorShield, "ahserion_advance_corridor_shield");

// Java AdvanceCorridorShield.java:25-29
void AdvanceCorridorShield::handleAttack(runtime::Ptr<Creature> creature) {
	AhserionConstructAI::handleAttack(creature);
	broadcastAttack();
}

// Java AdvanceCorridorShield.java:31-43
void AdvanceCorridorShield::broadcastAttack() {
	if (canShout.compareAndSet(true, false)) {
		if (!isDead() && getOwner().getWorldId() == 400030000) {
			switch (getNpcId()) {
				case 297306:
					sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_PORTAL_DEST_69_ATTACKED());
					break;
				case 297307:
					sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_PORTAL_DEST_70_ATTACKED());
					break;
				case 297308:
					sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_PORTAL_DEST_71_ATTACKED());
					break;
				case 297309:
					sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_PORTAL_DEST_72_ATTACKED());
					break;
				default:
					break;
			}
			ThreadPoolManager::getInstance().schedule({this}, [this] { allowShout(); }, 3000);
		}
	}
}

// Java AdvanceCorridorShield.java:45-48
void AdvanceCorridorShield::allowShout() {
	if (!isDead())
		canShout.set(true);
}

// Java AdvanceCorridorShield.java:50-52
void AdvanceCorridorShield::sendPacket(network::aion::serverpackets::SM_SYSTEM_MESSAGE msg) {
	world::World::getInstance().getWorldMap(400030000)->getMainWorldMapInstance()->forEachPlayer(
		[&msg](Player& p) { PacketSendUtility::sendPacket(p, msg); });
}

// Java AdvanceCorridorShield.java:54-73
void AdvanceCorridorShield::handleDied() {
	if (getOwner().getWorldId() == 400030000 && AhserionRaid::getInstance().isStarted()) {
		switch (getNpcId()) {
			case 297306:
				sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_PORTAL_DEST_69_BROKEN());
				break;
			case 297307:
				sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_PORTAL_DEST_70_BROKEN());
				break;
			case 297308:
				sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_PORTAL_DEST_71_BROKEN());
				break;
			case 297309:
				sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_PORTAL_DEST_72_BROKEN());
				break;
			default:
				break;
		}
		AhserionRaid::getInstance().handleCorridorShieldDestruction(getNpcId());
	}
	AhserionConstructAI::handleDied();
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
