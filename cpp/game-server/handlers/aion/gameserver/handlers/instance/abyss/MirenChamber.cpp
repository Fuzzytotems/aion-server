#include "aion/gameserver/handlers/instance/abyss/MirenChamber.h"

namespace aion::gameserver::handlers::instance::abyss {

AION_INSTANCE_HANDLER(MirenChamber, 300130000);

// Java MirenChamber.java:16-18
MirenChamber::MirenChamber(WorldMapInstance& instance) : AbstractInnerUpperAbyssInstance(instance) {
}

runtime::Ref<MirenChamber> MirenChamber::create(WorldMapInstance& instance) {
	return runtime::makeRef<MirenChamber>(instance);
}

// Java MirenChamber.java:21-23
int32_t MirenChamber::getBossId() {
	return 215222;
}

// Java MirenChamber.java:26-28
int32_t MirenChamber::getChestId() {
	return 700543;
}

// Java MirenChamber.java:31-33
int32_t MirenChamber::getDoorId() {
	return 700547;
}

// Java MirenChamber.java:36-38
int32_t MirenChamber::getKeymasterId() {
	return 215216;
}

// Java MirenChamber.java:41-43
int32_t MirenChamber::getTimerNpcId() {
	return 206097;
}

// Java MirenChamber.java:46-65
void MirenChamber::onDie(Npc& npc) {
	AbstractInnerUpperAbyssInstance::onDie(npc);
	switch (npc.getNpcId()) {
		case 215188:
			openDoor(78);
			break;
		case 215189:
			openDoor(77);
			break;
		case 215190:
			openDoor(15);
			break;
		case 215191:
			openDoor(11);
			break;
		case 215200:
			openDoor(12);
			break;
		case 215201:
			openDoor(7);
			break;
		case 215202:
			openDoor(81);
			break;
		case 215203:
			openDoor(8);
			break;
		case 215212:
			openDoor(13);
			break;
		case 215213:
			openDoor(75);
			break;
		case 215214:
			openDoor(16);
			break;
		case 215215:
			openDoor(82);
			break;
		case 215220:
			openDoor(73);
			break;
		case 215221:
		case 215222:
			spawnChests();
			break;
		case 215415:
			switchToEasyMode();
			break;
		default:
			break;
	}
}

} // namespace aion::gameserver::handlers::instance::abyss
