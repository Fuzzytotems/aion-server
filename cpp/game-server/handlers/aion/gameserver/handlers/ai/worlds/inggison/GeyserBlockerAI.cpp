#include "aion/gameserver/handlers/ai/worlds/inggison/GeyserBlockerAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::handlers::ai::worlds::inggison {

AION_AI(GeyserBlockerAI, "geyserblocker");

// Java GeyserBlockerAI.java:29-38
void GeyserBlockerAI::handleDied() {
	AggressiveNpcAI::handleDied();
	if (staticId.get() == -1) // no geyser was despawned on spawn, so we cannot respawn it
		return;
	runtime::Ptr<model::templates::spawns::SpawnTemplate> spawnPoint = getOwner().getSpawn();
	spawn(700545, spawnPoint->getX(), spawnPoint->getY(), spawnPoint->getZ(), int8_t{0}, staticId.get());
	PacketSendUtility::broadcastPacket(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_WINDBOX_TRIGGER_ON_INFO(),
		[this](Player& player) { return PositionUtil::isInRange(getOwner(), player, 30); });
}

// Java GeyserBlockerAI.java:40-48. Correction of the Java code (owner's decision 2026-10-19, both branches; was J-A1C-1): the delete is a task
// that runs once the known list's update has returned. handleCreatureSee runs inside it (KnownList::update -> notifySee -> NpcController::see),
// under the NpcKnownList monitor, and World::removeObject takes the npc's: Java deleted the geyser right there, the reverse of the
// Npc -> NpcKnownList lock order (KnownList.update is synchronized, KnownList.java:43), which the port had to suppress.
void GeyserBlockerAI::handleCreatureSee(Creature& creature) {
	AggressiveNpcAI::handleCreatureSee(creature);
	if (runtime::Ptr<Npc> npc = runtime::as<Npc>(creature)) { // Java: instanceof Npc, then the cast
		if (npc->getNpcId() == 700545 && PositionUtil::isInRange(getOwner(), *npc, 5)) {
			staticId = npc->getSpawn()->getStaticId();
			Npc& geyser = *npc;
			ThreadPoolManager::getInstance().schedule({this, &geyser}, [&geyser] {
				if (geyser.isSpawned())
					geyser.getController().delete_();
			}, 0);
		}
	}
}

} // namespace aion::gameserver::handlers::ai::worlds::inggison
