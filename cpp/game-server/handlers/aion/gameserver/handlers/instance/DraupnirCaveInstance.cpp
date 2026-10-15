#include "aion/gameserver/handlers/instance/DraupnirCaveInstance.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/flyring/FlyRing.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(DraupnirCaveInstance, 320080000);

// Java DraupnirCaveInstance.java:28-30
DraupnirCaveInstance::DraupnirCaveInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<DraupnirCaveInstance> DraupnirCaveInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<DraupnirCaveInstance>(instance);
}

// Java DraupnirCaveInstance.java:33-43 (the ring's template is immortal, as static data: FlyRingTemplate.h)
void DraupnirCaveInstance::onInstanceCreate() {
	runtime::Ref<Point3D> center = Point3D::create(475.8222f, 427.8852f, 618.3719f);
	runtime::Ref<Point3D> p1 = Point3D::create(478.3698f, 450.1829f, 624.6974f);
	runtime::Ref<Point3D> p2 = Point3D::create(473.2742f, 405.5875f, 612.0464f);
	const auto* flyRingTemplate = new FlyRingTemplate("INSTANCE_MSG_START", mapId, *center, *p1, *p2, 20);
	VisibleObject::create<FlyRing>(flyRingTemplate, instance->getInstanceId())->spawn();
	switch (commons::utils::Rnd::get(1, 4)) {
		case 1:
			spawn(213587, 567.438f, 700.875f, 538.701f, int8_t{7}); // Hungry Ooze
			break;
		case 2:
			spawn(213588, 166.8f, 536.285f, 505.802f, int8_t{9}); // Lucky Golden Saam
			break;
		case 3:
			spawn(213771, 497.006f, 434.713f, 616.584f, int8_t{71}); // Protector Rakkan
			break;
		case 4:
			spawn(213773, 380.694f, 611.956f, 598.523f, int8_t{98}); // Dragonpriest Tairgus
			break;
		default:
			break;
	}
}

// Java DraupnirCaveInstance.java:46-52
bool DraupnirCaveInstance::onPassFlyingRing(Player& /*player*/, std::string_view flyingRing) {
	if ("INSTANCE_MSG_START" == flyingRing && isInstanceStartMessageSend.compareAndSet(false, true)) {
		sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_BOSS_SPAWN_IDDF3_DRAGON_1());
		return true;
	}
	return false;
}

// Java DraupnirCaveInstance.java:55-72
void DraupnirCaveInstance::onDie(Npc& npc) {
	switch (npc.getNpcId()) {
		case 213776: // Instructor Afrane
		case 213778: // Beautiful Lakshmi
		case 213779: // Commander Nimbarka
		case 213802: // Kind Saraswati
			adjutantsKilled = adjutantsKilled + 1;
			switch (adjutantsKilled.get()) {
				case 1:
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_BOSS_SPAWN_IDDF3_DRAGON_2());
					break;
				case 2:
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_BOSS_SPAWN_IDDF3_DRAGON_3());
					break;
				case 3:
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_BOSS_SPAWN_IDDF3_DRAGON_4());
					break;
				case 4:
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_BOSS_SPAWN_IDDF3_DRAGON());
					spawn(213780, 813.189f, 432.255f, 318.928f, int8_t{30}); // Commander Bakarma
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
}

// Java DraupnirCaveInstance.java:75-81
bool DraupnirCaveInstance::onReviveEvent(Player& player) {
	PlayerReviveService::revive(player, 25, 25, true, 0);
	player.getGameStats()->updateStatsAndSpeedVisually();
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	TeleportService::teleportTo(player, *instance, 492.83383f, 375.46542f, 622.26920f, int8_t{29});
	return true;
}

} // namespace aion::gameserver::handlers::instance
