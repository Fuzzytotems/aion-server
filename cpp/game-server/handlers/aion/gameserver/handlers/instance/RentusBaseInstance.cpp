#include "aion/gameserver/handlers/instance/RentusBaseInstance.h"

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

AION_INSTANCE_HANDLER(RentusBaseInstance, 300280000);

// Java RentusBaseInstance.java:39-41
RentusBaseInstance::RentusBaseInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<RentusBaseInstance> RentusBaseInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<RentusBaseInstance>(instance);
}

// Java RentusBaseInstance.java:43-100
void RentusBaseInstance::onDie(Npc& npc) {
	GeneralInstanceHandler::onDie(npc);
	switch (npc.getNpcId()) {
		case 217315: // Umatha the Crazed
			if (isDeadNpc(217316)) {
				instance->setDoorState(145, true);
				deleteAliveNpcs({701156});
			}
			break;
		case 217316: // Ambusher Kiriana
			if (isDeadNpc(217315)) {
				instance->setDoorState(145, true);
				deleteAliveNpcs({701156});
			}
			break;
		case 217311: // Kuhara
			spawn(219215, 141.54f, 255.06f, 213.0f, int8_t{25});
			instance->setDoorState(43, false);
			instance->setDoorState(150, true);
			npc.getController().delete_();
			break;
		case 217317: // Archmagus Upadi
			instance->setDoorState(70, true);
			break;
		case 217313: // Brigade General Vasharti
			deleteAliveNpcs({799669});
			instance->setDoorState(70, true);
			spawn(730520, 193.6f, 436.5f, 262.0f, int8_t{86}); // rentus base exit
			spawn(833047, 195.48f, 413.87f, 260.97f, int8_t{27}); // rentus supplies storage box
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
		case 236283:
			spawn(236284, npc.getX(), npc.getY(), npc.getZ(), npc.getHeading());
			npc.getController().delete_();
			break;
		default:
			break;
	}
}

// Java RentusBaseInstance.java:102-113
void RentusBaseInstance::onAggro(Npc& npc) {
	switch (npc.getNpcId()) {
		case 217311: // Kuhara
			instance->setDoorState(43, true);
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(npc), 1500393);
			break;
		case 217313: // Vasharti
			instance->setDoorState(70, false);
			break;
		default:
			break;
	}
}

// Java RentusBaseInstance.java:115-117
void RentusBaseInstance::spawnEndEvent(int32_t npcId, std::string_view walkern, int32_t time) {
	sp(npcId, 193.39548f, 435.56158f, 260.57135f, int8_t{86}, time, walkern);
}

// Java RentusBaseInstance.java:119-150
void RentusBaseInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
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
		case 701100:
			if (instance->getNpc(799543) == nullptr)
				spawn(799543, 511.227f, 613.762f, 158.179f, int8_t{0});
			break;
		default:
			break;
	}
}

// Java RentusBaseInstance.java:152-160 (the task keeps the npc alive, as Java's capture does)
void RentusBaseInstance::sp(int32_t npcId, float x, float y, float z, int8_t h, int32_t time, std::string_view walkern) {
	const runtime::Ref<Npc> npc(*runtime::cast<Npc>(spawn(npcId, x, y, z, h)));
	ThreadPoolManager::getInstance().schedule({this}, [npc, walker = std::string(walkern)] {
		npc->getSpawn()->setWalkerId(walker);
		WalkManager::startWalking(dynamic_cast<gameserver::ai::NpcAI&>(npc->getAi())); // Java: (NpcAI) npc.getAi()
		npc->setState(CreatureState::WALK_MODE, true);
		PacketSendUtility::broadcastPacket(*npc, SM_EMOTION(*npc, EmotionType::CHANGE_SPEED, 0, npc->getObjectId()));
	}, time);
}

// Java RentusBaseInstance.java:162-164
bool RentusBaseInstance::isDeadNpc(int32_t npcId) {
	return (getNpc(npcId) == nullptr || getNpc(npcId)->isDead());
}

// Java RentusBaseInstance.java:166-176
void RentusBaseInstance::onCreatureDetected(Npc& detector, Creature& detected) {
	if (runtime::as<Player>(runtime::Ptr<Creature>(detected)) != nullptr) {
		if (detector.getNpcId() == 856056 && isXastaEventStarted.compareAndSet(false, true)) {
			sp(236271, 521.33f, 499.49f, 179.946f, int8_t{27}, 2000, "300620000_Xasta_Path");
			detector.getController().delete_();
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDYUN_RASTA_SPAWN_01());
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDYUN_RASTA_SPAWN_02(), 2000);
		}
	}
}

// Java RentusBaseInstance.java:178-190
void RentusBaseInstance::onSpecialEvent(Npc& npc) {
	if (npc.getNpcId() == 236271) {
		if (npc.getLifeStats()->getHpPercentage() <= 50)
			spawn(217310, 354.53f, 596.26f, 148.298f, int8_t{100});
		else
			spawn(217309, 354.53f, 596.26f, 148.298f, int8_t{100});
		runtime::Ptr<Npc> drakanBarricade = getNpc(856015);
		if (drakanBarricade != nullptr)
			drakanBarricade->getController().die(npc);
		npc.getController().delete_();
	}
}

// Java RentusBaseInstance.java:192-203
void RentusBaseInstance::onEnterInstance(Player& player) {
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

// Java RentusBaseInstance.java:205-211
bool RentusBaseInstance::isBoss(Npc& npc) {
	switch (npc.getNpcId()) {
		case 217309:
		case 217310:
		case 217311:
		case 217315:
		case 217316:
		case 217317:
		case 217313:
			return true;
		default:
			return false;
	}
}

} // namespace aion::gameserver::handlers::instance
