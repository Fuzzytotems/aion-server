#include "aion/gameserver/handlers/instance/LinkgateFoundryInstance.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(LinkgateFoundryInstance, 301270000);

// Java LinkgateFoundryInstance.java:29-31
LinkgateFoundryInstance::LinkgateFoundryInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<LinkgateFoundryInstance> LinkgateFoundryInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<LinkgateFoundryInstance>(instance);
}

// Java LinkgateFoundryInstance.java:33-39
void LinkgateFoundryInstance::onEnterInstance(Player& player) {
	if (!instanceRace.get()) {
		instanceRace = player.getRace();
		spawn(instanceRace.get() == Race::ELYOS ? 206361 : 206362, 348.00464f, 252.13882f, 311.36136f, int8_t{10});
	}
}

// Java LinkgateFoundryInstance.java:41-46
void LinkgateFoundryInstance::onDie(Npc& npc) {
	switch (npc.getNpcId()) {
		case 233898:
		case 234990:
		case 234991:
			spawn(instanceRace.get() == Race::ELYOS ? 702338 : 702389, 246.74345f, 258.35843f, 312.32327f, int8_t{10});
			break;
		default:
			break;
	}
}

// Java LinkgateFoundryInstance.java:48-75 (secretLabEntranceCount is a Java byte)
void LinkgateFoundryInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	switch (npc.getNpcId()) {
		case 804578:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF4_Re_01_Time_01()); // 20 min
			startTimeCheck();
			npc.getController().die(); // not a static door in client data o.O
			break;
		case 234193:
			npc.getController().die();
			break;
		case 804629:
			TeleportService::teleportTo(*player, 301270000, 228.37f, 262.7f, 313.0f, static_cast<int8_t>(120));
			break;
		case 702592:
			TeleportService::teleportTo(*player, 301270000, 211.32f, 260.0f, 314.0f, int8_t{0}, TeleportAnimation::FADE_OUT_BEAM);
			break;
		case 702590:
			TeleportService::teleportTo(*player, 301270000, 257.11f, 323.0f, 271.0f, int8_t{60}, TeleportAnimation::FADE_OUT_BEAM);
			npc.getController().delete_();
			secretLabEntranceCount = static_cast<int8_t>(secretLabEntranceCount + 1);
			if (secretLabEntranceCount.get() < 3) {
				spawn(234992, 244.1839f, 322.5356f, 270.9474f, int8_t{0});
			} else
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF4_Re_01_secret_room_03());
			break;
		default:
			break;
	}
}

// Java LinkgateFoundryInstance.java:77-100 (timeInMin is a Java byte)
void LinkgateFoundryInstance::startTimeCheck() {
	timeCheckTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		timeInMin = static_cast<int8_t>(timeInMin + 1);
		switch (timeInMin.get()) {
			case 5:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF4_Re_01_Time_02());
				break;
			case 10:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF4_Re_01_Time_03());
				break;
			case 15:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF4_Re_01_Time_04());
				break;
			case 17:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF4_Re_01_Time_05());
				break;
			case 19:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF4_Re_01_Time_06());
				break;
			case 20: {
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF4_Re_01_Time_07());
				instance->forEachNpc([](Npc& npc) {
					if (npc.getNpcId() != 233898 && npc.getNpcId() != 234990 && npc.getNpcId() != 234991 // belsagos does not despawn
						&& npc.getNpcId() != 702339 && npc.getNpcId() != 804629 // teleport device does not despawn
						&& npc.getNpcId() != 702590 && npc.getNpcId() != 702591 && npc.getNpcId() != 234992) { // secret lab portal & chest do not despawn
						npc.getController().delete_();
					}
				});
				runtime::FutureRef task = timeCheckTask.get();
				if (task != nullptr && !task->isDone()) {
					task->cancel(true);
				}
				break;
			}
			default:
				break;
		}
	}, 0, 60000));
}

// Java LinkgateFoundryInstance.java:102-109
void LinkgateFoundryInstance::onEnterZone(Player& player, ZoneInstance& zone) {
	if (zone.getAreaTemplate()->getZoneName() == ZoneName::get("IDLDF4RE_01_ITEMUSEAREA_BOSS_301270000")) {
		runtime::FutureRef task = timeCheckTask.get();
		if (task != nullptr && !task->isDone()) {
			task->cancel(true);
		}
	}
}

// Java LinkgateFoundryInstance.java:111-116
void LinkgateFoundryInstance::onInstanceDestroy() {
	runtime::FutureRef task = timeCheckTask.get();
	if (task != nullptr && !task->isDone()) {
		task->cancel(true);
	}
}

} // namespace aion::gameserver::handlers::instance
