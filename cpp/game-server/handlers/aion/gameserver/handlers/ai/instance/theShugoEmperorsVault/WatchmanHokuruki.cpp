#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/WatchmanHokuruki.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"

namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {

AION_AI(WatchmanHokuruki, "watchman_hokuruki");

// Java WatchmanHokuruki.java:19-25
WatchmanHokuruki::WatchmanHokuruki(Npc& owner) : IDSweep_Bosses(owner) {
	randomPositions.add(1);
	randomPositions.add(2);
	randomPositions.add(3);
	randomPositions.shuffle();
}

// Java WatchmanHokuruki.java:27-31
void WatchmanHokuruki::handleAttack(runtime::Ptr<Creature> creature) {
	IDSweep_Bosses::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java WatchmanHokuruki.java:33-41
void WatchmanHokuruki::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 100:
		case 15:
			rndSpawn(235632, 4);
			break;
		case 75:
			spawnAdds(randomPositions.get(0));
			break;
		case 50:
			spawnAdds(randomPositions.get(1));
			break;
		case 25:
			spawnAdds(randomPositions.get(2));
			break;
		default:
			break;
	}
}

// Java WatchmanHokuruki.java:43-61
void WatchmanHokuruki::spawnAdds(int32_t position) {
	switch (position) {
		case 1:
			spawn(236083, 472.8545f, 644.94f, 395.50546f, int8_t{113});
			spawn(235649, 473.7f, 648.9f, 395.55615f, int8_t{116});
			spawn(235649, 471.75f, 641.0f, 395.27322f, int8_t{116});
			break;
		case 2:
			spawn(236083, 490.0f, 655.1424f, 394.69073f, int8_t{84});
			spawn(235649, 485.812f, 656.14f, 395.375f, int8_t{90});
			spawn(235649, 493.59f, 652.62f, 395.41345f, int8_t{77});
			break;
		case 3:
			spawn(236083, 493.0f, 629.34f, 394.89963f, int8_t{41});
			spawn(235649, 496.56f, 631.78f, 395.20126f, int8_t{49});
			spawn(235649, 488.86f, 627.54f, 395.27853f, int8_t{38});
			break;
		default:
			break;
	}
}

// Java WatchmanHokuruki.java:63-68
void WatchmanHokuruki::handleBackHome() {
	IDSweep_Bosses::handleBackHome();
	hpPhases->reset();
	randomPositions.shuffle();
}

// Java WatchmanHokuruki.java:70-74
void WatchmanHokuruki::handleDied() {
	IDSweep_Bosses::handleDied();
	getOwner().getController().delete_();
}

// Java WatchmanHokuruki.java:76-82 (Java's addHate(null, 1) without a most hated is a no-op: AggroList.isAware)
void WatchmanHokuruki::rndSpawn(int32_t npcId, int32_t count) {
	runtime::Ptr<Creature> mostHated = getAggroList().getTarget(AggroTarget::MOST_HATED);
	for (int32_t i = 0; i < count; i++) {
		runtime::Ptr<Npc> npc = runtime::cast<Npc>(rndSpawnInRange(npcId, 3, 5));
		if (mostHated != nullptr)
			npc->getAggroList().addHate(*mostHated, 1);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
