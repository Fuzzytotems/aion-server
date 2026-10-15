#include "aion/gameserver/handlers/instance/abyss/KrotanChamber.h"

namespace aion::gameserver::handlers::instance::abyss {

AION_INSTANCE_HANDLER(KrotanChamber, 300140000);

// Java KrotanChamber.java:16-18
KrotanChamber::KrotanChamber(WorldMapInstance& instance) : AbstractInnerUpperAbyssInstance(instance) {
}

runtime::Ref<KrotanChamber> KrotanChamber::create(WorldMapInstance& instance) {
	return runtime::makeRef<KrotanChamber>(instance);
}

// Java KrotanChamber.java:21-23
int32_t KrotanChamber::getBossId() {
	return 215136;
}

// Java KrotanChamber.java:26-28
int32_t KrotanChamber::getChestId() {
	return 700539;
}

// Java KrotanChamber.java:31-33
int32_t KrotanChamber::getDoorId() {
	return 700545;
}

// Java KrotanChamber.java:36-38
int32_t KrotanChamber::getKeymasterId() {
	return 215130;
}

// Java KrotanChamber.java:41-43
int32_t KrotanChamber::getTimerNpcId() {
	return 206095;
}

// Java KrotanChamber.java:46-65
void KrotanChamber::onDie(Npc& npc) {
	AbstractInnerUpperAbyssInstance::onDie(npc);
	switch (npc.getNpcId()) {
		case 215102:
			openDoor(10);
			break;
		case 215103:
			openDoor(5);
			break;
		case 215104:
			openDoor(30);
			break;
		case 215105:
			openDoor(6);
			break;
		case 215114:
			openDoor(32);
			break;
		case 215115:
			openDoor(31);
			break;
		case 215116:
			openDoor(12);
			break;
		case 215117:
			openDoor(29);
			break;
		case 215126:
			openDoor(13);
			break;
		case 215127:
			openDoor(8);
			break;
		case 215128:
			openDoor(7);
			break;
		case 215129:
			openDoor(9);
			break;
		case 215134:
			openDoor(16);
			break;
		case 215135:
		case 215136:
			spawnChests();
			break;
		case 215413:
			switchToEasyMode();
			break;
		default:
			break;
	}
}

} // namespace aion::gameserver::handlers::instance::abyss
