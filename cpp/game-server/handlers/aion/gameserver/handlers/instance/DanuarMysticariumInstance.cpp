#include "aion/gameserver/handlers/instance/DanuarMysticariumInstance.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(DanuarMysticariumInstance, 300480000);

// Java DanuarMysticariumInstance.java:42-44
DanuarMysticariumInstance::DanuarMysticariumInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<DanuarMysticariumInstance> DanuarMysticariumInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<DanuarMysticariumInstance>(instance);
}

// Java DanuarMysticariumInstance.java:46-58
void DanuarMysticariumInstance::onOpenDoor(int32_t door) {
	switch (door) {
		case 6:
			spawn(219964, 225.53f, 529.7f, 153.04f, static_cast<int8_t>(100));
			break;
		case 7:
			spawn(219963, 241.602f, 541.79f, 152.591f, static_cast<int8_t>(95));
			break;
		case 8:
			spawn(219965, 262.17f, 545.68f, 150.51f, static_cast<int8_t>(85));
			break;
		case 10:
			spawn(219964, 295.04f, 547.48f, 148.73f, static_cast<int8_t>(90));
			break;
		case 11:
			spawn(219963, 317.654f, 545.801f, 148.8f, static_cast<int8_t>(80));
			break;
		case 12:
			spawn(219965, 336.94f, 532.89f, 148.472f, static_cast<int8_t>(75));
			break;
		case 13:
			spawn(219969, 348.14f, 512.56f, 148.19f, static_cast<int8_t>(65));
			break;
		case 101:
			spawn(219963, 212.068f, 510.02f, 153.23f, static_cast<int8_t>(115));
			break;
		default:
			break;
	}
}

// Java DanuarMysticariumInstance.java:60-77
void DanuarMysticariumInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	switch (npc.getNpcId()) {
		case 731583:
			startTasks();
			instance->setDoorState(3, true);
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5Re_solo_game1_1());
			TeleportService::teleportTo(*player, *instance, 140.45f, 182.2f, 242.0f, int8_t{10}, TeleportAnimation::FADE_OUT_BEAM);
			npc.getController().delete_();
			break;
		case 702715:
			TeleportService::teleportTo(*player, *instance, 236.1f, 488.86f, 152.0f, int8_t{25}, TeleportAnimation::FADE_OUT_BEAM);
			break;
		case 702717:
			TeleportService::moveToInstanceExit(*player, mapId, player->getRace());
			break;
		default:
			break;
	}
}

// Java DanuarMysticariumInstance.java:79-90
void DanuarMysticariumInstance::startTasks() {
	tasks.set(runtime::RcArrayList<runtime::FutureRef>::create(AION_LOCK_CLASS(DanuarMysticariumInstance::tasks)));
	runtime::Ptr<runtime::RcArrayList<runtime::FutureRef>> list = tasks.get();
	list->add(ThreadPoolManager::getInstance().schedule({this}, [this] { sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5Re_solo_game1_2()); }, 125000));
	list->add(ThreadPoolManager::getInstance().schedule({this}, [this] { sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5Re_solo_game1_3()); }, 155000));
	list->add(ThreadPoolManager::getInstance().schedule({this}, [this] { sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5Re_solo_game1_4()); }, 175000));
	list->add(ThreadPoolManager::getInstance().schedule({this}, [this] {
		deleteAliveNpcs({219958, 219959, 702700, 702701});
		spawn(702715, 169.366f, 208.93f, 188.02f, int8_t{0});
		sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5Re_solo_game1_5());
		sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5Re_solo_game1_6());
	}, 185000)); // 3min 5sec
}

// Java DanuarMysticariumInstance.java:92-96
void DanuarMysticariumInstance::cancelTasks() {
	runtime::Ptr<runtime::RcArrayList<runtime::FutureRef>> list = tasks.get();
	if (list != nullptr) {
		for (const runtime::FutureRef& future : list->snapshot()) {
			if (future != nullptr && !future->isCancelled())
				future->cancel(true);
		}
	}
}

// Java DanuarMysticariumInstance.java:98-101
void DanuarMysticariumInstance::onInstanceDestroy() {
	cancelTasks();
}

// Java DanuarMysticariumInstance.java:103-106
void DanuarMysticariumInstance::leaveInstance(Player& player) {
	TeleportService::moveToInstanceExit(player, mapId, player.getRace());
}

} // namespace aion::gameserver::handlers::instance
