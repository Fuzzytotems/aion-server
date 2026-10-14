#include "aion/gameserver/handlers/instance/OphidanBridgeInstance.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(OphidanBridgeInstance, 300590000);

// Java OphidanBridgeInstance.java:33-35
OphidanBridgeInstance::OphidanBridgeInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<OphidanBridgeInstance> OphidanBridgeInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<OphidanBridgeInstance>(instance);
}

// Java OphidanBridgeInstance.java:37-42
void OphidanBridgeInstance::onInstanceDestroy() {
	runtime::FutureRef current = task.get();
	if (current != nullptr && !current->isCancelled()) {
		current->cancel(true);
	}
}

// Java OphidanBridgeInstance.java:44-86 (bossKills and endBossKills are Java bytes)
void OphidanBridgeInstance::onDie(Npc& npc) {
	GeneralInstanceHandler::onDie(npc);
	switch (npc.getNpcId()) {
		case 235764: // Escapee Asachin, triggers normal mode
			isNormalMode.set(true);
			break;
		case 235775:
		case 235774:
		case 235773:
		case 235772:
			if (!isNormalMode.get()) {
				deleteAliveNpcs({235764});
			}
			bossKills = static_cast<int8_t>(bossKills + 1);
			if (bossKills.get() == 3) {
				spawn(701644, 436.99045f, 496.55634f, 605.95203f, int8_t{2}); // Bridge Control
				if (!isNormalMode.get()) {
					spawnRandomBosses();
				} else {
					spawn(235768, 323.016f, 489.295f, 607.645f, int8_t{0}); // Spirited Velkur
				}
			}
			break;
		case 235759:
		case 235763:
		case 235767:
		case 235768:
		case 235769:
		case 235770:
		case 235771:
			endBossKills = static_cast<int8_t>(endBossKills + 1);
			if (endBossKills.get() == (isNormalMode.get() ? 1 : 2)) {
				spawn(730868, 315.85f, 488.65f, 607.6435f, int8_t{0}); // Ophidan Bridge exit
			} else {
				scheduleEnrage();
			}
			break;
		case 235786: // Steel Wall
			npc.getController().delete_();
			break;
		default:
			break;
	}
}

// Java OphidanBridgeInstance.java:88-93
void OphidanBridgeInstance::scheduleEnrage() {
	task.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (endBossKills.get() < 2)
			spawn(856059, 322.4960f, 488.3117f, 656.4463f, int8_t{50});
	}, 10000));
}

// Java OphidanBridgeInstance.java:95-103
void OphidanBridgeInstance::spawnRandomBosses() {
	int32_t npcId = 235759 + (commons::utils::Rnd::get(0, 2) * 4);
	spawn(npcId, 325.1554f, 483.4476f, 607.6434f, int8_t{0});
	switch (npcId) {
		case 235759:
			spawn(235769, 323.016f, 489.295f, 607.645f, int8_t{0}); // Velkur Aethercaster
			break;
		case 235763:
			spawn(235770, 323.016f, 489.295f, 607.645f, int8_t{0}); // Velkur Aetherpriest
			break;
		case 235767:
			spawn(235771, 323.016f, 489.295f, 607.645f, int8_t{0}); // Velkur Aetherknife
			break;
		default:
			break;
	}
}

// Java OphidanBridgeInstance.java:105-121
void OphidanBridgeInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	switch (npc.getNpcId()) {
		case 701644:
			if (used.compareAndSet(false, true)) {
				npc.getController().delete_();
				spawn(731544, 436.36f, 496.45f, 604.8871f, int8_t{2}, 7);
			}
			break;
		case 731544:
			TeleportService::teleportTo(*player, player->getWorldId(), 369.532f, 491.7861f, 605.696f, int8_t{60}, TeleportAnimation::FADE_OUT_BEAM);
			break;
		case 730868:
			InstanceService::moveToExitPoint(*player);
			break;
		default:
			break;
	}
}

// Java OphidanBridgeInstance.java:123-130
bool OphidanBridgeInstance::onReviveEvent(Player& player) {
	PlayerReviveService::revive(player, 25, 25, true, 0);
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	player.getGameStats()->updateStatsAndSpeedVisually();
	TeleportService::teleportTo(player, *instance, 755.21f, 559.292f, 572.9508f, int8_t{86});
	return true;
}

} // namespace aion::gameserver::handlers::instance
