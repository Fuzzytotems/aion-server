#include "aion/gameserver/handlers/instance/DanuarReliquaryInstance.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(DanuarReliquaryInstance, 301110000);

// Java DanuarReliquaryInstance.java:28-30
DanuarReliquaryInstance::DanuarReliquaryInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<DanuarReliquaryInstance> DanuarReliquaryInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<DanuarReliquaryInstance>(instance);
}

// Java DanuarReliquaryInstance.java:32-34
int32_t DanuarReliquaryInstance::getExitId() {
	return 730843;
}

// Java DanuarReliquaryInstance.java:36-38
int32_t DanuarReliquaryInstance::getTreasureBoxId() {
	return 701795;
}

// Java DanuarReliquaryInstance.java:40-42
int32_t DanuarReliquaryInstance::getEnragedModorId() {
	return 231305;
}

// Java DanuarReliquaryInstance.java:44-46
int32_t DanuarReliquaryInstance::getCursedModorId() {
	return 231304;
}

// Java DanuarReliquaryInstance.java:48-50
int32_t DanuarReliquaryInstance::getRealCloneId() {
	return 284383; // 855244
}

// Java DanuarReliquaryInstance.java:52-54
int32_t DanuarReliquaryInstance::getFakeCloneId() {
	return 284384; // 855244
}

// Java DanuarReliquaryInstance.java:56-98
void DanuarReliquaryInstance::onDie(Npc& npc) {
	GeneralInstanceHandler::onDie(npc);
	const int32_t npcId = npc.getNpcId();
	switch (npcId) {
		case 284377: // Idean Obscura
		case 284378: // Idean Lapilima
		case 284379: { // Danuar Reliquary Novun
			npc.getController().delete_();
			bool allDead = true; // Java: instance.getNpcs(284377, 284378, 284379).stream().allMatch(Creature::isDead)
			for (const runtime::Ptr<Npc>& guardian : instance->getNpcs({284377, 284378, 284379})) {
				if (!guardian->isDead()) {
					allDead = false;
					break;
				}
			}
			if (allDead && isCursedModorActive.compareAndSet(false, true)) {
				spawn(getCursedModorId(), 256.62f, 257.79f, 241.79f, int8_t{90});
				runtime::Ptr<Npc> cursedModor = getNpc(getCursedModorId());
				if (cursedModor != nullptr) {
					// SkillEngine.getInstance().getSkill(cursedModor, 21168, 1, cursedModor).useWithoutPropSkill();
					// sendMsg(SM_SYSTEM_MESSAGE.STR_MSG_IDLDF5_INDER_RUNE_START());
					scheduleWipe();
				}
			}
			break;
		}
		case 284383: // Modor's Clone
		case 855244: // Modor's Clone
			deleteAliveNpcs({getFakeCloneId()});
			npc.getController().delete_();
			ThreadPoolManager::getInstance().schedule({this}, [this] {
				if (cloneKills.incrementAndGet() >= 3) {
					spawn(getEnragedModorId(), 256.62f, 257.79f, 241.79f, int8_t{90});
				} else {
					spawnClones();
				}
			}, 2000);
			break;
		case 231305: // Enraged Queen Modor
		case 234691: // Crazed Modor
			onInstanceEnd(true);
			break;
		case 701795: // Treasure Box
		case 802183:
			break;
		default:
			npc.getController().delete_();
			break;
	}
}

// Java DanuarReliquaryInstance.java:100-120
void DanuarReliquaryInstance::spawnClones() {
	int32_t spawnCase = commons::utils::Rnd::get(1, 5);
	switch (spawnCase) {
		case 1:
			spawn(getRealCloneId(), 255.5489f, 293.42154f, 253.78925f, int8_t{90});
			break;
		case 2:
			spawn(getRealCloneId(), 232.5363f, 263.90112f, 248.65384f, int8_t{114});
			break;
		case 3:
			spawn(getRealCloneId(), 240.11194f, 235.08876f, 251.14906f, int8_t{17});
			break;
		case 4:
			spawn(getRealCloneId(), 271.23627f, 230.30913f, 250.92981f, int8_t{42});
			break;
		case 5:
			spawn(getRealCloneId(), 284.6919f, 262.7201f, 248.75252f, int8_t{63});
			break;
		default:
			break;
	}

	if (spawnCase != 1)
		spawn(getFakeCloneId(), 255.5489f, 293.42154f, 253.78925f, int8_t{90});
	if (spawnCase != 2)
		spawn(getFakeCloneId(), 232.5363f, 263.90112f, 248.65384f, int8_t{114});
	if (spawnCase != 3)
		spawn(getFakeCloneId(), 240.11194f, 235.08876f, 251.14906f, int8_t{17});
	if (spawnCase != 4)
		spawn(getFakeCloneId(), 271.23627f, 230.30913f, 250.92981f, int8_t{42});
	if (spawnCase != 5)
		spawn(getFakeCloneId(), 284.6919f, 262.7201f, 248.75252f, int8_t{63});
}

// Java DanuarReliquaryInstance.java:122-139
void DanuarReliquaryInstance::onInstanceEnd(bool successful) {
	cancelWipeTask();
	runtime::Ptr<Npc> modor = getNpc(getEnragedModorId());
	if (modor == nullptr) {
		modor = getNpc(getCursedModorId());
	}
	if (modor != nullptr) {
		if (successful) {
			PacketSendUtility::broadcastMessage(modor, 343629);
		} else {
			PacketSendUtility::broadcastMessage(modor, 1500739);
		}
	}
	instance->forEachNpc([](Npc& npc) { npc.getController().delete_(); });
	spawn(getExitId(), 255.66669f, 263.78525f, 241.7986f, int8_t{86}); // Spawn exit portal
	if (successful)
		spawn(getTreasureBoxId(), 256.65f, 258.09f, 241.78f, int8_t{100}); // Treasure Box
}

// Java DanuarReliquaryInstance.java:141-146
void DanuarReliquaryInstance::scheduleWipe() {
	wipeTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		spawn(284386, 256.60f, 257.99f, 241.78f, int8_t{0});
		onInstanceEnd(false);
	}, 15 * 60000));
}

// Java DanuarReliquaryInstance.java:148-151
void DanuarReliquaryInstance::cancelWipeTask() {
	runtime::FutureRef task = wipeTask.get();
	if (task != nullptr && !task->isCancelled())
		task->cancel(false);
}

// Java DanuarReliquaryInstance.java:153-158
void DanuarReliquaryInstance::onPlayerLogout(Player& player) {
	GeneralInstanceHandler::onPlayerLogout(player);
	if (player.isDead())
		TeleportService::moveToBindLocation(player);
}

// Java DanuarReliquaryInstance.java:160-163
void DanuarReliquaryInstance::onInstanceDestroy() {
	cancelWipeTask();
}

// Java DanuarReliquaryInstance.java:165-174
void DanuarReliquaryInstance::onBackHome(Npc& npc) {
	if (npc.getNpcId() == getEnragedModorId() || npc.getNpcId() == getCursedModorId()) {
		const int32_t homeId = npc.getNpcId();
		instance->forEachNpc([homeId](Npc& other) {
			if (other.getNpcId() != homeId) {
				other.getController().delete_();
			}
		});
	}
}

} // namespace aion::gameserver::handlers::instance
