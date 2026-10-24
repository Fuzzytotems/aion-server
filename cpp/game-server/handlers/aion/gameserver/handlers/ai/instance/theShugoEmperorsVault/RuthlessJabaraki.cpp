#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/RuthlessJabaraki.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"

namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {

AION_AI(RuthlessJabaraki, "ruthless_jabaraki");

// Java RuthlessJabaraki.java:25-29
void RuthlessJabaraki::handleAttack(runtime::Ptr<Creature> creature) {
	IDSweep_Bosses::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java RuthlessJabaraki.java:31-41 (every case is a phase since the correction of 2026-10-19, was J-I6-2)
void RuthlessJabaraki::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 95:
			spawnAdds(1);
			break;
		case 80:
			spawnAdds(1);
			break;
		case 65:
			spawnAdds(2);
			break;
		case 50:
			spawnAdds(3);
			break;
		case 35:
			spawnAdds(4);
			break;
		case 20:
			spawnAdds(5);
			break;
		default:
			break;
	}
}

// Java RuthlessJabaraki.java:43-107
void RuthlessJabaraki::spawnAdds(int32_t stage) {
	switch (stage) {
		case 1:
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235631, 541.625f, 407.0712f, 395.46875f, int8_t{110}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235631, 538.86127f, 389.05258f, 395.4342f, int8_t{8}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235631, 558.6854f, 382.02362f, 395.16785f, int8_t{43}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235631, 564.9021f, 400.29718f, 395.46262f, int8_t{69}))));
			break;
		case 2:
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 541.625f, 407.0712f, 395.46875f, int8_t{110}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 538.86127f, 389.05258f, 395.4342f, int8_t{8}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 558.6854f, 382.02362f, 395.16785f, int8_t{43}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 564.9021f, 400.29718f, 395.46262f, int8_t{69}))));

			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 540.625f, 406.0712f, 395.76875f, int8_t{110}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 539.86127f, 390.05258f, 395.7342f, int8_t{8}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 559.6854f, 383.02362f, 395.46785f, int8_t{43}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 565.9021f, 401.29718f, 395.76262f, int8_t{69}))));
			break;
		case 3:
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235629, 541.625f, 407.0712f, 395.46875f, int8_t{110}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235629, 538.86127f, 389.05258f, 395.4342f, int8_t{8}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235629, 558.6854f, 382.02362f, 395.16785f, int8_t{43}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235629, 564.9021f, 400.29718f, 395.46262f, int8_t{69}))));

			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235629, 540.625f, 406.0712f, 395.76875f, int8_t{110}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235629, 539.86127f, 390.05258f, 395.7342f, int8_t{8}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235629, 559.6854f, 383.02362f, 395.46785f, int8_t{43}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235629, 565.9021f, 401.29718f, 395.76262f, int8_t{69}))));
			break;
		case 4:
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 541.625f, 407.0712f, 395.46875f, int8_t{110}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 538.86127f, 389.05258f, 395.4342f, int8_t{8}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 558.6854f, 382.02362f, 395.16785f, int8_t{43}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 564.9021f, 400.29718f, 395.46262f, int8_t{69}))));

			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 540.625f, 406.0712f, 395.76875f, int8_t{110}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 539.86127f, 390.05258f, 395.7342f, int8_t{8}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 559.6854f, 383.02362f, 395.46785f, int8_t{43}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 565.9021f, 401.29718f, 395.76262f, int8_t{69}))));

			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 541.625f, 407.0712f, 396.06875f, int8_t{110}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 540.86127f, 391.05258f, 396.0342f, int8_t{8}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 560.6854f, 384.02362f, 395.86785f, int8_t{43}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235630, 566.9021f, 402.29718f, 395.86262f, int8_t{69}))));
			break;
		case 5:
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235631, 541.625f, 407.0712f, 395.46875f, int8_t{110}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235631, 538.86127f, 389.05258f, 395.4342f, int8_t{8}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235631, 558.6854f, 382.02362f, 395.16785f, int8_t{43}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235631, 564.9021f, 400.29718f, 395.46262f, int8_t{69}))));

			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235631, 540.625f, 406.0712f, 395.76875f, int8_t{110}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235631, 539.86127f, 390.05258f, 395.7342f, int8_t{8}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235631, 559.6854f, 383.02362f, 395.46785f, int8_t{43}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235631, 565.9021f, 401.29718f, 395.76262f, int8_t{69}))));

			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235631, 541.625f, 407.0712f, 396.06875f, int8_t{110}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235631, 540.86127f, 391.05258f, 396.0342f, int8_t{8}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235631, 560.6854f, 384.02362f, 395.86785f, int8_t{43}))));
			spawnedAdds.add(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(235631, 566.9021f, 402.29718f, 395.86262f, int8_t{69}))));
			break;
		default:
			break;
	}
	startHate();
}

// Java RuthlessJabaraki.java:109-116 (Java's addHate(null, 1) without a most hated is a no-op: AggroList.isAware)
void RuthlessJabaraki::startHate() {
	runtime::Ptr<Creature> mostHated = getAggroList().getTarget(AggroTarget::MOST_HATED);
	for (runtime::Ptr<Npc> npc : spawnedAdds) {
		if (npc != nullptr && !npc->isDead()) {
			if (mostHated != nullptr)
				npc->getAggroList().addHate(*mostHated, 1);
		}
	}
}

// Java RuthlessJabaraki.java:118-122
void RuthlessJabaraki::handleBackHome() {
	IDSweep_Bosses::handleBackHome();
	hpPhases->reset();
}

// Java RuthlessJabaraki.java:124-128
void RuthlessJabaraki::handleDied() {
	IDSweep_Bosses::handleDied();
	getOwner().getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
