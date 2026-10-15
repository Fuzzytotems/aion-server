#include "aion/gameserver/handlers/instance/abyss/KysisBarracks.h"

namespace aion::gameserver::handlers::instance::abyss {

AION_INSTANCE_HANDLER(KysisBarracks, 301280000);

// Java KysisBarracks.java:16-18
KysisBarracks::KysisBarracks(WorldMapInstance& instance) : AbstractInnerUpperAbyssInstance(instance) {
}

runtime::Ref<KysisBarracks> KysisBarracks::create(WorldMapInstance& instance) {
	return runtime::makeRef<KysisBarracks>(instance);
}

// Java KysisBarracks.java:21-23
int32_t KysisBarracks::getBossId() {
	return 233676;
}

// Java KysisBarracks.java:26-28
int32_t KysisBarracks::getChestId() {
	return 702292;
}

// Java KysisBarracks.java:31-33
int32_t KysisBarracks::getDoorId() {
	return 700546;
}

// Java KysisBarracks.java:36-38
int32_t KysisBarracks::getKeymasterId() {
	return 233670;
}

// Java KysisBarracks.java:41-43
int32_t KysisBarracks::getTimerNpcId() {
	return 206096;
}

// Java KysisBarracks.java:46-68
void KysisBarracks::onDie(Npc& npc) {
	AbstractInnerUpperAbyssInstance::onDie(npc);
	switch (npc.getNpcId()) {
		case 233642:
			openDoor(5);
			break;
		case 233643:
			openDoor(10);
			break;
		case 233644:
			openDoor(30);
			break;
		case 233645:
			openDoor(6);
			break;
		case 233654:
			openDoor(29);
			break;
		case 233655:
			openDoor(31);
			break;
		case 233656:
			openDoor(12);
			break;
		case 233657:
			openDoor(32);
			break;
		case 233666:
			openDoor(8);
			break;
		case 233667:
			openDoor(9);
			break;
		case 233668:
			openDoor(13);
			break;
		case 233669:
			openDoor(7);
			break;
		case 233674:
			openDoor(16);
			break;
		case 233675:
		case 233676:
			spawnChests();
			break;
		case 215414:
			switchToEasyMode();
			break;
		case 235536:
			rewardStatueKill(3000);
			break;
		case 235537:
			rewardStatueKill(6000);
			break;
		case 235538:
			rewardStatueKill(12000);
			break;
		default:
			break;
	}
}

} // namespace aion::gameserver::handlers::instance::abyss
