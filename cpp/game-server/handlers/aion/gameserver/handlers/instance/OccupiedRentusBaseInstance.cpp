#include "aion/gameserver/handlers/instance/OccupiedRentusBaseInstance.h"

#include <string>

#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h" // the Ref<Effect> applyEffectDirectly returns
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(OccupiedRentusBaseInstance, 300620000);

// Java OccupiedRentusBaseInstance.java:32-34
OccupiedRentusBaseInstance::OccupiedRentusBaseInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<OccupiedRentusBaseInstance> OccupiedRentusBaseInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<OccupiedRentusBaseInstance>(instance);
}

// Java OccupiedRentusBaseInstance.java:36-93
void OccupiedRentusBaseInstance::onDie(Npc& npc) {
	GeneralInstanceHandler::onDie(npc);
	switch (npc.getNpcId()) {
		case 236299: // Umatha the Crazed
			if (isDeadNpc(236301)) {
				instance->setDoorState(145, true);
				deleteAliveNpcs({701156});
			}
			break;
		case 236301: // Ambusher Kiriana
			if (isDeadNpc(236299)) {
				instance->setDoorState(145, true);
				deleteAliveNpcs({701156});
			}
			break;
		case 236298: // Kuhara
			spawn(236705, 141.54f, 255.06f, 213.0f, int8_t{25});
			instance->setDoorState(43, false);
			instance->setDoorState(150, true);
			npc.getController().delete_();
			break;
		case 236302: // Archmagus Upadi
			instance->setDoorState(70, true);
			break;
		case 236300: // Brigade General Vasharti
			deleteAliveNpcs({799669});
			instance->setDoorState(70, true);
			spawn(730401, 193.6f, 436.5f, 262.0f, int8_t{86}); // occupied rentus base exit
			spawn(833048, 195.48f, 413.87f, 260.97f, int8_t{27}); // Rentus Quality Supply Storage Box
			{
				runtime::Ptr<Npc> ariana = runtime::cast<Npc>(spawn(799670, 183.736f, 391.392f, 260.571f, int8_t{26}));
				PacketSendUtility::broadcastMessage(ariana, 1500417, 5000);
				PacketSendUtility::broadcastMessage(ariana, 1500418, 8000);
				PacketSendUtility::broadcastMessage(ariana, 1500419, 11000);
			}
			spawnEndEvent(800227, "3002800003", 2000);
			spawnEndEvent(800227, "3002800004", 2000);
			spawnEndEvent(800228, "3002800007", 4000);
			spawnEndEvent(800227, "3002800005", 6000);
			spawnEndEvent(800228, "3002800006", 8000);
			spawnEndEvent(800229, "3002800008", 10000);
			spawnEndEvent(800229, "3002800009", 10000);
			spawnEndEvent(800230, "30028000010", 12000);
			spawnEndEvent(800230, "30028000011", 12000);
			break;
		case 282394:
			spawn(282395, npc.getX(), npc.getY(), npc.getZ(), npc.getHeading());
			npc.getController().delete_();
			break;
		case 283000:
		case 283001:
			npc.getController().delete_();
			break;
		case 217299:
			spawn(217300, npc.getX(), npc.getY(), npc.getZ(), npc.getHeading());
			npc.getController().delete_();
			break;
		default:
			break;
	}
}

// Java OccupiedRentusBaseInstance.java:95-106
void OccupiedRentusBaseInstance::onAggro(Npc& npc) {
	switch (npc.getNpcId()) {
		case 236298: // Kuhara
			instance->setDoorState(43, true);
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(npc), 1500393);
			break;
		case 236300: // Vasharti
			instance->setDoorState(70, false);
			break;
		default:
			break;
	}
}

// Java OccupiedRentusBaseInstance.java:108-110
void OccupiedRentusBaseInstance::spawnEndEvent(int32_t npcId, std::string_view walkern, int32_t time) {
	sp(npcId, 193.39548f, 435.56158f, 260.57135f, int8_t{86}, time, walkern);
}

// Java OccupiedRentusBaseInstance.java:112-151
void OccupiedRentusBaseInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	switch (npc.getNpcId()) {
		case 702677:
		case 702678:
		case 702679:
		case 702680:
		case 702681:
		case 702682:
			TeleportService::teleportTo(*player, npc.getWorldId(), npc.getInstanceId(), npc.getX(), npc.getY(), npc.getZ(), npc.getHeading());
			SkillEngine::getInstance().applyEffectDirectly(21806, npc, *player);
			npc.getController().delete_();
			break;
		case 702683:
		case 702684:
		case 702685:
		case 702686:
		case 702687:
		case 702688:
			TeleportService::teleportTo(*player, npc.getWorldId(), npc.getInstanceId(), npc.getX(), npc.getY(), npc.getZ(), npc.getHeading());
			SkillEngine::getInstance().applyEffectDirectly(21805, npc, *player);
			npc.getController().delete_();
			break;
		case 701097:
			npc.getController().delete_();
			break;
		case 701151:
			SkillEngine::getInstance().getSkill(npc, 19909, 60, runtime::Ptr<VisibleObject>(npc))->useNoAnimationSkill();
			npc.getController().delete_();
			break;
		case 701152:
			SkillEngine::getInstance().getSkill(npc, 19910, 60, runtime::Ptr<VisibleObject>(npc))->useNoAnimationSkill();
			npc.getController().delete_();
			break;
		case 701100:
			if (instance->getNpc(799543) == nullptr)
				spawn(799543, 511.227f, 613.762f, 158.179f, int8_t{0});
			break;
		default:
			break;
	}
}

// Java OccupiedRentusBaseInstance.java:153-161 (the task keeps the npc alive, as Java's capture does)
void OccupiedRentusBaseInstance::sp(int32_t npcId, float x, float y, float z, int8_t h, int32_t time, std::string_view walkern) {
	const runtime::Ref<Npc> npc(*runtime::cast<Npc>(spawn(npcId, x, y, z, h)));
	ThreadPoolManager::getInstance().schedule({this}, [npc, walker = std::string(walkern)] {
		npc->getSpawn()->setWalkerId(walker);
		WalkManager::startWalking(dynamic_cast<gameserver::ai::NpcAI&>(npc->getAi())); // Java: (NpcAI) npc.getAi()
		npc->setState(CreatureState::WALK_MODE, true);
		PacketSendUtility::broadcastPacket(*npc, SM_EMOTION(*npc, EmotionType::CHANGE_SPEED, 0, npc->getObjectId()));
	}, time);
}

// Java OccupiedRentusBaseInstance.java:163-165
bool OccupiedRentusBaseInstance::isDeadNpc(int32_t npcId) {
	return (getNpc(npcId) == nullptr || getNpc(npcId)->isDead());
}

// Java OccupiedRentusBaseInstance.java:167-177
void OccupiedRentusBaseInstance::onCreatureDetected(Npc& detector, Creature& detected) {
	if (runtime::as<Player>(runtime::Ptr<Creature>(detected)) != nullptr) {
		if (detector.getNpcId() == 856056 && isXastaEventStarted.compareAndSet(false, true)) {
			sp(236271, 521.33f, 499.49f, 179.946f, int8_t{27}, 2000, "300620000_Xasta_Path");
			detector.getController().delete_();
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDYUN_RASTA_SPAWN_01());
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDYUN_RASTA_SPAWN_02(), 2000);
		}
	}
}

// Java OccupiedRentusBaseInstance.java:179-191
void OccupiedRentusBaseInstance::onSpecialEvent(Npc& npc) {
	if (npc.getNpcId() == 236271) {
		if (npc.getLifeStats()->getHpPercentage() <= 50)
			spawn(236297, 354.53f, 596.26f, 148.298f, int8_t{100});
		else
			spawn(236296, 354.53f, 596.26f, 148.298f, int8_t{100});
		runtime::Ptr<Npc> drakanBarricade = getNpc(856015);
		if (drakanBarricade != nullptr)
			drakanBarricade->getController().die(npc);
		npc.getController().delete_();
	}
}

// Java OccupiedRentusBaseInstance.java:193-204
void OccupiedRentusBaseInstance::onEnterInstance(Player& player) {
	if (isRaceKnown.compareAndSet(false, true)) {
		if (player.getRace() == Race::ELYOS) {
			for (int32_t npcId = 702677; npcId <= 702682; npcId++) {
				runtime::Ptr<Npc> npc = getNpc(npcId);
				spawn(npcId + 6, npc->getX(), npc->getY(), npc->getZ(), npc->getHeading(), npc->getSpawn()->getStaticId());
				npc->getController().delete_();
			}
		}
	}
}

// Java OccupiedRentusBaseInstance.java:206-212
bool OccupiedRentusBaseInstance::isBoss(Npc& npc) {
	switch (npc.getNpcId()) {
		case 236296:
		case 236297:
		case 236298:
		case 236299:
		case 236300:
		case 236301:
		case 236302:
			return true;
		default:
			return false;
	}
}

} // namespace aion::gameserver::handlers::instance
