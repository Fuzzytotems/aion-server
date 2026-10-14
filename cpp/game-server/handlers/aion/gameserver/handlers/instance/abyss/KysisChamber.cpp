#include "aion/gameserver/handlers/instance/abyss/KysisChamber.h"

namespace aion::gameserver::handlers::instance::abyss {

AION_INSTANCE_HANDLER(KysisChamber, 300120000);

// Java KysisChamber.java:16-18
KysisChamber::KysisChamber(WorldMapInstance& instance) : AbstractInnerUpperAbyssInstance(instance) {
}

runtime::Ref<KysisChamber> KysisChamber::create(WorldMapInstance& instance) {
	return runtime::makeRef<KysisChamber>(instance);
}

// Java KysisChamber.java:21-23
int32_t KysisChamber::getBossId() {
	return 215179;
}

// Java KysisChamber.java:26-28
int32_t KysisChamber::getChestId() {
	return 700541;
}

// Java KysisChamber.java:31-33
int32_t KysisChamber::getDoorId() {
	return 700546;
}

// Java KysisChamber.java:36-38
int32_t KysisChamber::getKeymasterId() {
	return 215173;
}

// Java KysisChamber.java:41-43
int32_t KysisChamber::getTimerNpcId() {
	return 206096;
}

// Java KysisChamber.java:46-65
void KysisChamber::onDie(Npc& npc) {
	AbstractInnerUpperAbyssInstance::onDie(npc);
	switch (npc.getNpcId()) {
		case 215145:
			openDoor(5);
			break;
		case 215146:
			openDoor(10);
			break;
		case 215147:
			openDoor(30);
			break;
		case 215148:
			openDoor(6);
			break;
		case 215157:
			openDoor(29);
			break;
		case 215158:
			openDoor(31);
			break;
		case 215159:
			openDoor(12);
			break;
		case 215160:
			openDoor(32);
			break;
		case 215169:
			openDoor(8);
			break;
		case 215170:
			openDoor(9);
			break;
		case 215171:
			openDoor(13);
			break;
		case 215172:
			openDoor(7);
			break;
		case 215177:
			openDoor(16);
			break;
		case 215178:
		case 215179:
			spawnChests();
			break;
		case 215414:
			switchToEasyMode();
			break;
		default:
			break;
	}
}

} // namespace aion::gameserver::handlers::instance::abyss
