#include "aion/gameserver/handlers/instance/abyss/MirenBarracks.h"

namespace aion::gameserver::handlers::instance::abyss {

AION_INSTANCE_HANDLER(MirenBarracks, 301290000);

// Java MirenBarracks.java:16-18
MirenBarracks::MirenBarracks(WorldMapInstance& instance) : AbstractInnerUpperAbyssInstance(instance) {
}

runtime::Ref<MirenBarracks> MirenBarracks::create(WorldMapInstance& instance) {
	return runtime::makeRef<MirenBarracks>(instance);
}

// Java MirenBarracks.java:21-23
int32_t MirenBarracks::getBossId() {
	return 233719;
}

// Java MirenBarracks.java:26-28
int32_t MirenBarracks::getChestId() {
	return 702296;
}

// Java MirenBarracks.java:31-33
int32_t MirenBarracks::getDoorId() {
	return 700547;
}

// Java MirenBarracks.java:36-38
int32_t MirenBarracks::getKeymasterId() {
	return 233713;
}

// Java MirenBarracks.java:41-43
int32_t MirenBarracks::getTimerNpcId() {
	return 206097;
}

// Java MirenBarracks.java:46-68
void MirenBarracks::onDie(Npc& npc) {
	AbstractInnerUpperAbyssInstance::onDie(npc);
	switch (npc.getNpcId()) {
		case 233685:
			openDoor(78);
			break;
		case 233686:
			openDoor(77);
			break;
		case 233687:
			openDoor(15);
			break;
		case 233688:
			openDoor(11);
			break;
		case 233697:
			openDoor(12);
			break;
		case 233698:
			openDoor(7);
			break;
		case 233699:
			openDoor(81);
			break;
		case 233700:
			openDoor(8);
			break;
		case 233709:
			openDoor(13);
			break;
		case 233710:
			openDoor(75);
			break;
		case 233711:
			openDoor(16);
			break;
		case 233712:
			openDoor(82);
			break;
		case 233717:
			openDoor(73);
			break;
		case 233718:
		case 233719:
			spawnChests();
			break;
		case 215415:
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
