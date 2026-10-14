#include "aion/gameserver/handlers/instance/abyss/AbstractInnerUpperAbyssInstance.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::instance::abyss {

// Java AbstractInnerUpperAbyssInstance.java:68-70
AbstractInnerUpperAbyssInstance::AbstractInnerUpperAbyssInstance(WorldMapInstance& instance)
	: GeneralInstanceHandler(instance),
	  chestLocations(runtime::RcArrayList<runtime::Ref<WorldPosition>>::create(AION_LOCK_CLASS(AbstractInnerUpperAbyssInstance::chestLocations))) {
}

// Java AbstractInnerUpperAbyssInstance.java:103-118
void AbstractInnerUpperAbyssInstance::onInstanceCreate() {
	spawn(getKeymasterId() + commons::utils::Rnd::nextInt(4), 527.64f, 212.0511f, 178.4134f, int8_t{90});
	runtime::RcArrayList<runtime::Ref<WorldPosition>>& locations = *chestLocations.get();
	locations.add(WorldPosition::create(mapId, 575.664f, 853.248f, 199.3737f, int8_t{63}));
	locations.add(WorldPosition::create(mapId, 571.560f, 869.936f, 199.3737f, int8_t{69}));
	locations.add(WorldPosition::create(mapId, 560.082f, 882.979f, 199.3737f, int8_t{76}));
	locations.add(WorldPosition::create(mapId, 545.404f, 892.116f, 199.3737f, int8_t{83}));
	locations.add(WorldPosition::create(mapId, 528.269f, 895.106f, 199.3737f, int8_t{89}));
	locations.add(WorldPosition::create(mapId, 511.364f, 891.941f, 199.3737f, int8_t{96}));
	locations.add(WorldPosition::create(mapId, 496.222f, 883.099f, 199.3737f, int8_t{103}));
	locations.add(WorldPosition::create(mapId, 484.933f, 869.897f, 199.3747f, int8_t{109}));
	locations.add(WorldPosition::create(mapId, 479.122f, 853.576f, 199.3727f, int8_t{116}));
	locations.add(WorldPosition::create(mapId, 478.966f, 836.407f, 199.3737f, int8_t{2}));
	locations.add(WorldPosition::create(mapId, 485.365f, 820.350f, 199.4596f, int8_t{9}));
	locations.add(WorldPosition::create(mapId, 576.463f, 837.337f, 199.7000f, int8_t{56})); // Bonus Chest
}

// Java AbstractInnerUpperAbyssInstance.java:121-136
void AbstractInnerUpperAbyssInstance::onCreatureDetected(Npc& detector, Creature& detected) {
	if (detector.getNpcId() == getTimerNpcId() && runtime::as<Player>(runtime::Ptr<Creature>(detected)) != nullptr &&
		isFlyringPassed.compareAndSet(false, true)) {
		chestReductionTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
			if (failCounter.incrementAndGet() >= 11) {
				deleteAliveNpcs({getBossId()});
				runtime::FutureRef task = chestReductionTask.get();
				if (task != nullptr && !task->isCancelled())
					task->cancel(true);
			}
		}, 300000, 30000));
		PacketSendUtility::broadcastToMap(*instance, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_START_IDABRE());
		PacketSendUtility::broadcastToMap(*instance, SM_QUEST_ACTION(0, 600));
		detector.getController().delete_();
	}
}

// Java AbstractInnerUpperAbyssInstance.java:143-145
void AbstractInnerUpperAbyssInstance::openDoor(int32_t staticId) {
	for (const runtime::Ptr<Npc>& n : instance->getNpcs({getDoorId()})) {
		if (n->getSpawn()->getStaticId() == staticId)
			n->getController().delete_();
	}
}

// Java AbstractInnerUpperAbyssInstance.java:150-158
void AbstractInnerUpperAbyssInstance::switchToEasyMode() {
	if (isEasyMode.compareAndSet(false, true)) {
		runtime::Ptr<Npc> boss = getNpc(getBossId());
		if (boss != nullptr && !boss->isDead()) {
			spawn(getBossId() - 1, boss->getX(), boss->getY(), boss->getZ(), boss->getHeading());
			boss->getController().delete_();
		}
	}
}

// Java AbstractInnerUpperAbyssInstance.java:164-180 (a second call finds chestLocations null: Java's NullPointerException)
void AbstractInnerUpperAbyssInstance::spawnChests() {
	runtime::FutureRef task = chestReductionTask.get();
	if (task != nullptr && !task->isCancelled())
		task->cancel(true);
	int32_t chestCount = 11 - failCounter.get();
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<WorldPosition>>> locations = chestLocations.get();
	if (locations == nullptr) // Java: chestLocations.get(i) of the null list (the second boss death after the first)
		throw runtime::NullPointerException("AbstractInnerUpperAbyssInstance.chestLocations");
	for (int32_t i = 0; i < chestCount; i++) {
		runtime::Ptr<WorldPosition> chestLoc = locations->get(i);
		spawn(getChestId(), chestLoc->getX(), chestLoc->getY(), chestLoc->getZ(), chestLoc->getHeading());
	}
	if (!isEasyMode.get()) {
		runtime::Ptr<WorldPosition> bChestLoc = locations->get(locations->size() - 1);
		spawn(getChestId() + 1, bChestLoc->getX(), bChestLoc->getY(), bChestLoc->getZ(), bChestLoc->getHeading());
	}
	locations->clear();
	chestLocations = nullptr;
}

// Java AbstractInnerUpperAbyssInstance.java:187-190
void AbstractInnerUpperAbyssInstance::rewardStatueKill(int32_t ap) {
	int32_t rewardAp = ap / instance->getPlayerCount();
	instance->forEachPlayer([rewardAp](Player& p) { AbyssPointsService::addAp(p, rewardAp); });
}

// Java AbstractInnerUpperAbyssInstance.java:193-195
bool AbstractInnerUpperAbyssInstance::isBoss(Npc& npc) {
	return npc.getNpcId() == getBossId();
}

} // namespace aion::gameserver::handlers::instance::abyss
