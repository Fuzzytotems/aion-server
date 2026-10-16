#include "aion/gameserver/handlers/ai/worlds/inggison/GeyserBlockerAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
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

// Java GeyserBlockerAI.java:40-48. The delete runs inside the known list's update (KnownList::update -> notifySee -> NpcController::see), under
// the NpcKnownList monitor, and World::removeObject takes the npc's: the reverse of the recorded Npc -> NpcKnownList order. Java has the same
// inversion (KnownList.update and clear are synchronized, KnownList.java:43/51); the port keeps it and suppresses the report (m5g-plan.md D4's
// way). Proposed correction J-A1C-1 (owner question, not applied): defer the delete to a task.
void GeyserBlockerAI::handleCreatureSee(Creature& creature) {
	AggressiveNpcAI::handleCreatureSee(creature);
	if (runtime::Ptr<Npc> npc = runtime::as<Npc>(creature)) { // Java: instanceof Npc, then the cast
		if (npc->getNpcId() == 700545 && PositionUtil::isInRange(getOwner(), *npc, 5)) {
			staticId = npc->getSpawn()->getStaticId();
			runtime::LockdepSuppression suppression("Java deletes the geyser inside see(), under the known list's lock (KnownList.java:43)");
			npc->getController().delete_();
		}
	}
}

} // namespace aion::gameserver::handlers::ai::worlds::inggison
