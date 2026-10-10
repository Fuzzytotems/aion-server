#include "aion/gameserver/handlers/ai/ConquestOfferingSpawnerAI.h"

#include <cstdint>

#include "aion/commons/utils/Rnd.h"

namespace aion::gameserver::handlers::ai {

AION_AI(ConquestOfferingSpawnerAI, "conquest_offering_spawner");

// Java ConquestOfferingSpawnerAI.java:27-31
void ConquestOfferingSpawnerAI::handleSpawned() {
	NpcAI::handleSpawned();
	spawnRandomNpc();
}

// Java ConquestOfferingSpawnerAI.java:33-86
void ConquestOfferingSpawnerAI::spawnRandomNpc() {
	switch (getNpcId()) {
		// Inggison
		case 856150:
		case 856156:
			spawnOffering(236307, 236335);
			break;
		case 856151:
		case 856157:
			spawnOffering(236311, 236339);
			break;
		case 856152:
		case 856158:
			spawnOffering(236315, 236343);
			break;
		case 856153:
		case 856159:
			spawnOffering(236319, 236347);
			break;
		case 856154:
		case 856160:
			spawnOffering(236323, 236351);
			break;
		case 856155:
		case 856161:
			spawnOffering(236327, 236355);
			break;
		// Gelkmaros
		case 856162:
		case 856168:
			spawnOffering(236363, 236391);
			break;
		case 856163:
		case 856169:
			spawnOffering(236367, 236395);
			break;
		case 856164:
		case 856170:
			spawnOffering(236371, 236399);
			break;
		case 856165:
		case 856171:
			spawnOffering(236375, 236403);
			break;
		case 856166:
		case 856172:
			spawnOffering(236379, 236407);
			break;
		case 856167:
		case 856173:
			spawnOffering(236383, 236411);
			break;
		default:
			break;
	}
}

// Java ConquestOfferingSpawnerAI.java:88-111: Ncsoft calls them "normal" and "party"
void ConquestOfferingSpawnerAI::spawnOffering(int32_t startNormal, int32_t startParty) {
	int32_t npcId;
	// calculate what kind of npc will be spawned 'normal'(70%) and 'party' (30%)
	if (commons::utils::Rnd::chance() < 70) {
		// theres another kind of spawn called 'all'(30%)
		if (commons::utils::Rnd::chance() < 30) {
			npcId = getRndNpc(startNormal);
		} else {
			npcId = startNormal + commons::utils::Rnd::get(0, 3);
		}
	} else {
		// theres another kind of spawn called 'all'(30%)
		if (commons::utils::Rnd::chance() < 30) {
			npcId = getRndNpc(startNormal);
		} else {
			npcId = startParty + commons::utils::Rnd::get(0, 3);
		}
	}
	if (npcId != 0)
		spawn(npcId, getOwner().getX(), getOwner().getY(), getOwner().getZ(), getOwner().getHeading());
}

// Java ConquestOfferingSpawnerAI.java:113-124
int32_t ConquestOfferingSpawnerAI::getRndNpc(int32_t curId) {
	if (curId <= 236327) { // normal
		return (236331 + commons::utils::Rnd::get(0, 3));
	} else if (curId <= 236355) { // party
		return (236359 + commons::utils::Rnd::get(0, 3));
	} else if (curId <= 236383) { // normal
		return (236387 + commons::utils::Rnd::get(0, 3));
	} else if (curId <= 236411) { // party
		return (236415 + commons::utils::Rnd::get(0, 3));
	}
	return 0;
}

// Java ConquestOfferingSpawnerAI.java:126-135. The respawn task is pinned on this AI (a part of its npc).
void ConquestOfferingSpawnerAI::handleCustomEvent(int32_t eventId, std::span<const std::any> /*args*/) {
	if (eventId == 1) { // spawned npc died, schedule respawn
		runtime::FutureRef task = respawnTask.get();
		if (task != nullptr && !task->isCancelled() && !task->isDone())
			return;
		int64_t respawnDelay = 600000 + (commons::utils::Rnd::get(0, 2) * int64_t{300000}); // random 10, 15 or 20 minutes
		respawnTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { spawnRandomNpc(); }, respawnDelay));
	}
}

// Java ConquestOfferingSpawnerAI.java:137-141
void ConquestOfferingSpawnerAI::handleDied() {
	NpcAI::handleDied();
	cancelTask();
}

// Java ConquestOfferingSpawnerAI.java:143-147
void ConquestOfferingSpawnerAI::handleDespawned() {
	cancelTask();
	NpcAI::handleDespawned();
}

// Java ConquestOfferingSpawnerAI.java:149-152
void ConquestOfferingSpawnerAI::cancelTask() {
	runtime::FutureRef task = respawnTask.get();
	if (task != nullptr && !task->isCancelled())
		task->cancel(false);
}

// Java ConquestOfferingSpawnerAI.java:154-157
float ConquestOfferingSpawnerAI::modifyDamage(Creature& /*attacker*/, float /*damage*/, runtime::Ptr<Effect> /*effect*/) {
	return 0;
}

// Java ConquestOfferingSpawnerAI.java:159-162
float ConquestOfferingSpawnerAI::modifyOwnerDamage(float /*damage*/, Creature& /*effected*/, runtime::Ptr<Effect> /*effect*/) {
	return 0;
}

} // namespace aion::gameserver::handlers::ai
