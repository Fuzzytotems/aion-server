#include "aion/gameserver/handlers/ai/instance/nightmareCircus/HarlequinLordReshkaAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

AION_AI(HarlequinLordReshkaAI, "harlequinlordreshka");

// Java HarlequinLordReshkaAI.java:27-33
void HarlequinLordReshkaAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	spawnTerrorsBox();
	spawnNightmaresBox();
	openBoxes();
}

// Java HarlequinLordReshkaAI.java:35-40
void HarlequinLordReshkaAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelTasks();
	despawnNpcs({831348, 831349});
}

// Java HarlequinLordReshkaAI.java:42-46
void HarlequinLordReshkaAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	cancelTasks();
}

// Java HarlequinLordReshkaAI.java:48-57
void HarlequinLordReshkaAI::cancelTasks() {
	for (int32_t i = 0; i < openBoxesTasks->length(); i++) {
		runtime::FutureRef openBoxesTask = openBoxesTasks->get(i);
		if (openBoxesTask != nullptr && !openBoxesTask->isDone()) {
			openBoxesTask->cancel(true);
		}
	}
	runtime::FutureRef boxes = spawnBoxesTask.get();
	if (boxes != nullptr && !boxes->isDone()) {
		boxes->cancel(true);
	}
}

// Java HarlequinLordReshkaAI.java:59-72
void HarlequinLordReshkaAI::spawnTerrorsBox() {
	spawn(831348, 507.42712f, 570.53394f, 199.50775f, int8_t{30});
	spawn(831348, 507.6568f, 560.18787f, 199.50775f, int8_t{30});
	spawn(831348, 512.9201f, 552.13983f, 199.50775f, int8_t{30});
	spawn(831348, 513.1767f, 577.7641f, 199.50775f, int8_t{30});
	spawn(831348, 521.76257f, 548.68469f, 199.50775f, int8_t{30});
	spawn(831348, 521.97913f, 583.61383f, 199.50775f, int8_t{30});
	spawn(831348, 529.46533f, 582.23792f, 199.50775f, int8_t{30});
	spawn(831348, 530.72772f, 549.67969f, 199.50775f, int8_t{30});
	spawn(831348, 536.98706f, 578.31433f, 199.50775f, int8_t{30});
	spawn(831348, 539.05304f, 552.27924f, 199.50775f, int8_t{30});
	spawn(831348, 542.64111f, 560.79907f, 200.3221f, int8_t{30});
	spawn(831348, 543.323f, 572.56665f, 199.50775f, int8_t{30});
}

// Java HarlequinLordReshkaAI.java:74-91
void HarlequinLordReshkaAI::spawnNightmaresBox() {
	spawnBoxesTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			spawn(831349, 507.42712f, 570.53394f, 199.50775f, int8_t{30});
			spawn(831349, 507.6568f, 560.18787f, 199.50775f, int8_t{30});
			spawn(831349, 512.9201f, 552.13983f, 199.50775f, int8_t{30});
			spawn(831349, 513.1767f, 577.7641f, 199.50775f, int8_t{30});
			spawn(831349, 521.76257f, 548.68469f, 199.50775f, int8_t{30});
			spawn(831349, 521.97913f, 583.61383f, 199.50775f, int8_t{30});
			spawn(831349, 529.46533f, 582.23792f, 199.50775f, int8_t{30});
			spawn(831349, 530.72772f, 549.67969f, 199.50775f, int8_t{30});
			spawn(831349, 536.98706f, 578.31433f, 199.50775f, int8_t{30});
			spawn(831349, 539.05304f, 552.27924f, 199.50775f, int8_t{30});
			spawn(831349, 542.64111f, 560.79907f, 200.3221f, int8_t{30});
			spawn(831349, 543.323f, 572.56665f, 199.50775f, int8_t{30});
		}
	}, 36000));
}

// Java HarlequinLordReshkaAI.java:93-107
void HarlequinLordReshkaAI::openBoxes() {
	(*openBoxesTasks)[0] = ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			AIActions::useSkill(*this, 21477);
			PacketSendUtility::broadcastMessage(getOwner(), 1501146);
		}
	}, 30000);
	(*openBoxesTasks)[1] = ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			AIActions::useSkill(*this, 21477);
			PacketSendUtility::broadcastMessage(getOwner(), 1501147);
			PacketSendUtility::broadcastMessage(getOwner(), commons::utils::Rnd::nextBoolean() ? 1501145 : 1501137, 2000);
		}
	}, 60000);
}

// Java HarlequinLordReshkaAI.java:109-113
void HarlequinLordReshkaAI::despawnNpcs(std::initializer_list<int32_t> npcIds) {
	for (const runtime::Ptr<Npc>& npc : getOwner().getPosition()->getWorldMapInstance()->getNpcs(npcIds)) {
		npc->getController().delete_();
	}
}

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
