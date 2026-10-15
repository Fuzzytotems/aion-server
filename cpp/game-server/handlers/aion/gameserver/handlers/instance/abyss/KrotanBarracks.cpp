#include "aion/gameserver/handlers/instance/abyss/KrotanBarracks.h"

namespace aion::gameserver::handlers::instance::abyss {

AION_INSTANCE_HANDLER(KrotanBarracks, 301300000);

// Java KrotanBarracks.java:16-18
KrotanBarracks::KrotanBarracks(WorldMapInstance& instance) : AbstractInnerUpperAbyssInstance(instance) {
}

runtime::Ref<KrotanBarracks> KrotanBarracks::create(WorldMapInstance& instance) {
	return runtime::makeRef<KrotanBarracks>(instance);
}

// Java KrotanBarracks.java:21-23
int32_t KrotanBarracks::getBossId() {
	return 233633;
}

// Java KrotanBarracks.java:26-28
int32_t KrotanBarracks::getChestId() {
	return 702288;
}

// Java KrotanBarracks.java:31-33
int32_t KrotanBarracks::getDoorId() {
	return 700545;
}

// Java KrotanBarracks.java:36-38
int32_t KrotanBarracks::getKeymasterId() {
	return 233627;
}

// Java KrotanBarracks.java:41-43
int32_t KrotanBarracks::getTimerNpcId() {
	return 206095;
}

// Java KrotanBarracks.java:46-68
void KrotanBarracks::onDie(Npc& npc) {
	AbstractInnerUpperAbyssInstance::onDie(npc);
	switch (npc.getNpcId()) {
		case 233599:
			openDoor(10);
			break;
		case 233600:
			openDoor(5);
			break;
		case 233601:
			openDoor(30);
			break;
		case 233602:
			openDoor(6);
			break;
		case 233611:
			openDoor(32);
			break;
		case 233612:
			openDoor(31);
			break;
		case 233613:
			openDoor(12);
			break;
		case 233614:
			openDoor(29);
			break;
		case 233623:
			openDoor(13);
			break;
		case 233624:
			openDoor(8);
			break;
		case 233625:
			openDoor(7);
			break;
		case 233626:
			openDoor(9);
			break;
		case 233631:
			openDoor(16);
			break;
		case 233632:
		case 233633:
			spawnChests();
			break;
		case 215413:
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
