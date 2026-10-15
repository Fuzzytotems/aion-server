#include "aion/gameserver/handlers/instance/abyss/EsoterraceInstance.h"

#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::handlers::instance::abyss {

AION_INSTANCE_HANDLER(EsoterraceInstance, 300250000);

// Java EsoterraceInstance.java:25-27
EsoterraceInstance::EsoterraceInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<EsoterraceInstance> EsoterraceInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<EsoterraceInstance>(instance);
}

// Java EsoterraceInstance.java:30-36
void EsoterraceInstance::onInstanceCreate() {
	instance->setDoorState(367, true);
	if (commons::utils::Rnd::chance() < 21) {
		spawn(799580, 1034.11f, 985.01f, 327.35095f, int8_t{105});
		spawn(217649, 1033.67f, 978.08f, 327.35095f, int8_t{35});
	}
}

// Java EsoterraceInstance.java:39-95
void EsoterraceInstance::onDie(Npc& npc) {
	switch (npc.getObjectTemplate()->getTemplateId()) {
		case 282295:
			instance->setDoorState(39, true);
			break;
		case 282291: // Surkana Feeder enables "hardmode"
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDF4Re_Drana_08());
			deleteAliveNpcs({217204});
			spawn(217205, 1315.43f, 1171.04f, 51.8054f, int8_t{66});
			break;
		case 217289:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDF4Re_Drana_07());
			instance->setDoorState(122, true);
			break;
		case 217281:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDF4Re_Drana_04());
			instance->setDoorState(70, true);
			break;
		case 217195:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDF4Re_Drana_05());
			instance->setDoorState(45, true);
			instance->setDoorState(52, true);
			instance->setDoorState(67, true);
			spawn(701027, 751.513489f, 1136.021851f, 365.031158f, int8_t{60}, 41);
			spawn(701027, 829.620789f, 1134.330078f, 365.031281f, int8_t{60}, 77);
			break;
		case 217185:
			spawn(701023, 1264.862061f, 644.995178f, 296.831818f, int8_t{60}, 112);
			instance->setDoorState(367, false);
			break;
		case 217204:
			instance->setDoorState(78, true); // collapse walls of Kexkra's chamber
			spawn(205437, 1309.390259f, 1163.644287f, 51.493992f, int8_t{13});
			spawn(701027, 1318.669800f, 1180.467651f, 52.879887f, int8_t{75}, 727);
			break;
		case 217206:
			spawn(205437, 1309.390259f, 1163.644287f, 51.493992f, int8_t{13});
			spawn(701027, 1318.669800f, 1180.467651f, 52.879887f, int8_t{75}, 727);
			spawn(701027, 1325.484497f, 1173.198486f, 52.879887f, int8_t{75}, 726);
			break;
		case 217649: {
			// keening sirokin treasure chest
			runtime::Ptr<Npc> keeningSirokin = getNpc(799580);
			spawn(701025, 1038.63f, 987.74f, 328.356f, int8_t{0}, 725);
			if (keeningSirokin == nullptr) // Java: broadcastMessage of the null npc
				throw runtime::NullPointerException("getNpc(799580)");
			PacketSendUtility::broadcastMessage(keeningSirokin, 342359);
			keeningSirokin->getController().delete_();
			break;
		}
		case 217282:
		case 217283:
		case 217284: {
			bool allDead = true;
			for (const runtime::Ptr<Npc>& n : instance->getNpcs({217282, 217283, 217284}))
				allDead = allDead && n->isDead();
			if (allDead) {
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDF4Re_Drana_03());
				instance->setDoorState(111, true);
			}
			break;
		}
		default:
			break;
	}
}

// Java EsoterraceInstance.java:98-104
bool EsoterraceInstance::onReviveEvent(Player& player) {
	PlayerReviveService::revive(player, 25, 25, false, 0);
	player.getGameStats()->updateStatsAndSpeedVisually();
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	TeleportService::teleportTo(player, *instance, 384.57535f, 535.4073f, 321.6642f, int8_t{17});
	return true;
}

// Java EsoterraceInstance.java:107-111
void EsoterraceInstance::onEnterZone(Player& player, ZoneInstance& zone) {
	if (zone.getAreaTemplate()->getZoneName() == ZoneName::get("DRANA_PRODUCTION_LAB_300250000")) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE(1400919, std::vector<std::string>{}));
	}
}

} // namespace aion::gameserver::handlers::instance::abyss
