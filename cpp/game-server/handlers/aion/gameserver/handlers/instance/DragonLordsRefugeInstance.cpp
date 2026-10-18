#include "aion/gameserver/handlers/instance/DragonLordsRefugeInstance.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(DragonLordsRefugeInstance, 300520000);

// Java DragonLordsRefugeInstance.java:41-56
DragonLordsRefugeInstance::DragonLordsRefugeInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
	// Retail positions
	respawnLocations.add(WorldPosition::create(300520000, 493.72f, 507.67f, 240.5f, int8_t{0}));
	respawnLocations.add(WorldPosition::create(300520000, 497.52f, 526.03f, 240.5f, int8_t{0}));
	respawnLocations.add(WorldPosition::create(300520000, 513.02f, 524.57f, 240.5f, int8_t{0}));
	respawnLocations.add(WorldPosition::create(300520000, 499.57f, 510.19f, 240.5f, int8_t{0}));
	respawnLocations.add(WorldPosition::create(300520000, 513.22f, 508.36f, 240.5f, int8_t{0}));
	respawnLocations.add(WorldPosition::create(300520000, 505.05f, 525.03f, 240.5f, int8_t{0}));
	respawnLocations.add(WorldPosition::create(300520000, 498.65f, 519.93f, 240.5f, int8_t{0}));
	respawnLocations.add(WorldPosition::create(300520000, 511.44f, 515.36f, 240.5f, int8_t{0}));
	respawnLocations.add(WorldPosition::create(300520000, 495.27f, 517.07f, 240.5f, int8_t{0}));
	respawnLocations.add(WorldPosition::create(300520000, 492.62f, 525.37f, 240.5f, int8_t{0}));
	respawnLocations.add(WorldPosition::create(300520000, 507.56f, 520.36f, 240.5f, int8_t{0}));
	respawnLocations.add(WorldPosition::create(300520000, 508.20f, 506.85f, 240.5f, int8_t{0}));
}

runtime::Ref<DragonLordsRefugeInstance> DragonLordsRefugeInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<DragonLordsRefugeInstance>(instance);
}

// Java DragonLordsRefugeInstance.java:58-115
void DragonLordsRefugeInstance::onSpawn(VisibleObject& object) {
	if (Npc* npc = dynamic_cast<Npc*>(&object)) {
		switch (npc->getNpcId()) {
			case 219361: // Tiamat Dragon
				ThreadPoolManager::getInstance().schedule({this}, [this] {
					spawn(730673, 459.548f, 456.849f, 417.405f, int8_t{21});
					spawn(730674, 547.909f, 456.568f, 417.405f, int8_t{45});
					spawn(730675, 460.082f, 571.978f, 417.405f, int8_t{98});
					spawn(730676, 547.822f, 571.876f, 417.405f, int8_t{74});
				}, 60000);
				break;
			case 219362: // Tiamat Weakened Dragon
				sendMsg(SM_SYSTEM_MESSAGE::IDTIAMAT_TIAMAT_COUNTDOWN_START(), 2000);
				PacketSendUtility::broadcastToMap(*instance, SM_QUEST_ACTION(0, 1800));
				failTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
					runtime::Ptr<Npc> tiamat = getNpc(219362);
					if (tiamat != nullptr && !tiamat->isDead()) {
						endInstance({730625, 730633, 730634, 730635, 730636});
						sendMsg(SM_SYSTEM_MESSAGE::IDTIAMAT_TIAMAT_COUNTDOWN_OVER());
					}
				}, 1800000)); // 30'
				break;
			case 219488: // Kaisinel 1st Phase
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_2PHASE_START_LIGHT(), 20000);
				break;
			case 219489: // Kaisinel 2nd Phase
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_KAISINEL_2PHASE_DEADLYATK(), 3000);
				break;
			case 219490: // Kaisinel 3rd Phase
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_KAISINEL_2PHASE_GROGGY(), 15000);
				break;
			case 219491: // Marchutan 1st Phase
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_2PHASE_START_DARK(), 20000);
				break;
			case 219492: // Marchutan 2nd Phase
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_MARCHUTAN_2PHASE_DEADLYATK(), 3000);
				break;
			case 219493: // Marchutan 3rd Phase
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_MARCHUTAN_2PHASE_GROGGY(), 15000);
				break;
			case 283140:
				sendMsg(SM_SYSTEM_MESSAGE::STR_IDTIAMAT_TIAMAT_SPAWN_BLACKHOLE(), 2500);
				break;
			case 730673:
				ThreadPoolManager::getInstance().schedule({this}, [this] {
					spawn(283163, 463.0f, 568.0f, 417.405f, int8_t{105});
					spawn(283164, 545.0f, 568.0f, 417.405f, int8_t{78});
					spawn(283165, 545.0f, 461.0f, 417.405f, int8_t{46});
					spawn(283166, 463.0f, 461.0f, 417.405f, int8_t{17});
				}, 10000);
				break;
			case 730695: // Surkana
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_KALYNDI_SURKANA_SPAWN(), 2500);
				break;
		}
	}
}

// Java DragonLordsRefugeInstance.java:117-143
void DragonLordsRefugeInstance::onStartEffect(runtime::Ptr<Effect> effect) {
	switch (effect->getSkillId()) {
		case 20918:
			PacketSendUtility::broadcastMessage(getNpc(800341), 1500612);
			break;
		case 20920:
			if (progress.compareAndSet(1, 2)) {
				ThreadPoolManager::getInstance().schedule({this}, [this] { spawn(219488 + raceId.get() * 3, 504.0f, 515.0f, 417.405f, int8_t{60}); }, 6000);
				spawn(219532, 469.0f, 563.0f, 417.41f, int8_t{103});
				spawn(219535, 466.0f, 560.0f, 417.41f, int8_t{103});
				spawn(219533, 542.0f, 559.0f, 417.41f, int8_t{79});
				spawn(219538, 538.0f, 562.0f, 417.41f, int8_t{79});
				spawn(219534, 537.0f, 466.0f, 417.41f, int8_t{42});
				spawn(219537, 541.0f, 469.0f, 417.41f, int8_t{42});
				spawn(219536, 466.0f, 471.0f, 417.41f, int8_t{18});
				spawn(219539, 470.0f, 467.0f, 417.41f, int8_t{18});
			}
			break;
		case 20993:
		case 20994:
		case 20995:
		case 20996:
			sendMsg(SM_SYSTEM_MESSAGE::IDTIAMAT_TIAMAT_DRAKAN_ON_DIE());
			break;
	}
}

// Java DragonLordsRefugeInstance.java:145-165
void DragonLordsRefugeInstance::onEndEffect(Effect& effect) {
	switch (effect.getSkillId()) {
		case 20975:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_2PHASE_CLOSE_CRACK());
			break;
		case 20976:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_2PHASE_CLOSE_RAGE());
			break;
		case 20977:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_2PHASE_CLOSE_GRAVITY());
			break;
		case 20978:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_2PHASE_CLOSE_CRYSTAL());
			break;
		case 20983:
			if (runtime::cast<Npc>(effect.getEffector())->getNpcId() == 219361)
				endInstance({730625, 730633, 730634, 730635, 730636});
			break;
	}
}

// Java DragonLordsRefugeInstance.java:167-173
void DragonLordsRefugeInstance::endInstance(std::initializer_list<int32_t> despawnExceptions) {
	const std::vector<int32_t> exceptions(despawnExceptions);
	instance->forEachNpc([&exceptions](Npc& npc) {
		if (std::ranges::find(exceptions, npc.getNpcId()) == exceptions.end())
			npc.getController().delete_();
	});
	spawn(730630, 548.18683f, 514.54523f, 420.0f, int8_t{0}, 23);
}

// Java DragonLordsRefugeInstance.java:175-232
void DragonLordsRefugeInstance::onDie(Npc& npc) {
	GeneralInstanceHandler::onDie(npc);
	switch (npc.getNpcId()) {
		case 219359: // Calindi Flamelord
			deleteAliveNpcs({730695, 730696}); // Surkana
			ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::useSkill(getNpc(219360)->getAi(), 20919); }, 4000);
			ThreadPoolManager::getInstance().schedule({this}, [this] { deleteAliveNpcs({730694}); }, 6000); // Aetheric field
			break;
		case 219361: // Tiamat Dragon - killed by Empyrean Lord
			getNpc(730699)->getController().die(); // Animates roof destruction
			getNpc(730700)->getController().die();
			spawn(283134, 451.97f, 514.55f, 417.40436f, int8_t{0});
			spawn(219362, 451.97f, 514.55f, 417.40436f, int8_t{0});
			spawn(730704, 437.541f, 513.487f, 415.824f, int8_t{0}, 17); // Collapsed Debris impaling Tiamat
			break;
		case 219362:
			failTask.get()->cancel(true);
			PacketSendUtility::broadcastToMap(*instance, SM_QUEST_ACTION(0, 0));
			// TODO: play movie
			endInstance({219490, 219493, 219362, 730625, 730633, 730634, 730635, 730636});
			spawn(701542, 480.0f, 514.0f, 417.405f, int8_t{0}); // Treasure Chest
			spawn(800430, 506.8f, 511.4f, 417.405f, int8_t{60}); // Kahrun
			spawn(800350 + raceId.get() * 6, 506.7f, 518.4f, 417.405f, int8_t{60}); // human Kaisinel/Marchutan
			spawn(800464, 544.964f, 517.898f, 417.405f, int8_t{113});
			spawn(800465, 545.605f, 510.325f, 417.405f, int8_t{17});
			break;
		case 219365: // Incarnations
		case 219366:
		case 219367:
		case 219368:
			if (incarnationKills.incrementAndGet() == 4)
				handlePhaseProgress();
			SkillEngine::getInstance().applyEffectDirectly(npc.getNpcId() - 198386, npc, *getNpc(219361)); // 20979 - 20982
			break;
		case 283163: // Balaur Spiritualist
		case 283164:
		case 283165:
		case 283166: {
			runtime::Ptr<Npc> empyreanLord = getNpc(219488 + raceId.get() * 3);
			if (empyreanLord != nullptr)
				SkillEngine::getInstance().applyEffectDirectly(npc.getNpcId() - 262170, npc, *empyreanLord); // 20993 - 20996
			break;
		}
		case 219488: // Kaisinel 1st Phase
		case 219491: // Marchutan 1st Phase
			if (incarnationKills.get() == 4) // in rare cases the empyrean lord dies while the instance have to wait for progress
				return;
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_DEADLYHOWLING());
			for (int32_t i = 219365; i <= 219368; i++) {
				runtime::Ptr<Npc> incarnation = getNpc(i);
				if (incarnation != nullptr)
					AIActions::useSkill(incarnation->getAi(), 20983);
			}
			AIActions::useSkill(getNpc(219361)->getAi(), 20983);
			ThreadPoolManager::getInstance().schedule({this}, [this] {
				endInstance({730625, 730633, 730634, 730635, 730636, 730699, 730700});
			}, 5000);
			break;
	}
}

// Java DragonLordsRefugeInstance.java:234-245
void DragonLordsRefugeInstance::handlePhaseProgress() {
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_2PHASE_CLOSE_ALL());

	int32_t empyreanLordId = 219488 + raceId.get() * 3;
	getNpc(219361)->getEffectController()->removeEffect(20984); // Dispel Unbreakable Wing

	// schedule spawn of empyrean lords for final attack to tiamat before became exhausted
	ThreadPoolManager::getInstance().schedule({this}, [this, empyreanLordId] {
		deleteAliveNpcs({empyreanLordId});
		spawn(219489 + raceId.get() * 3, 516.285f, 514.84f, 417.405f, int8_t{60});
	}, 30000);
}

// Java DragonLordsRefugeInstance.java:250-269
void DragonLordsRefugeInstance::handleDialogueEvent(Npc& tiamat) {
	runtime::Ref<Npc> kahrun(*getNpc(800341));
	runtime::Ref<Npc> tiamatRef(tiamat);
	AIActions::targetCreature(tiamat.getAi(), *kahrun);
	PacketSendUtility::broadcastMessage(tiamatRef, 1500613, 5000);
	PacketSendUtility::broadcastMessage(kahrun, 1500609, 10000);
	PacketSendUtility::broadcastMessage(tiamatRef, 1500614, 15000);
	PacketSendUtility::broadcastMessage(kahrun, 1500610, 20000);
	PacketSendUtility::broadcastMessage(tiamatRef, 1500615, 26500);
	PacketSendUtility::broadcastMessage(kahrun, 1500611, 31500);
	ThreadPoolManager::getInstance().schedule({this}, [kahrun] { AIActions::useSkill(kahrun->getAi(), 20597); }, 35500);
	PacketSendUtility::broadcastMessage(tiamatRef, 1500616, 40000);
	ThreadPoolManager::getInstance().schedule({this}, [tiamatRef] { AIActions::useSkill(tiamatRef->getAi(), 20918); }, 49000);
	PacketSendUtility::broadcastMessage(tiamatRef, 1500617, 44500);
	ThreadPoolManager::getInstance().schedule({this}, [this, kahrun, tiamatRef] {
		kahrun->getController().delete_();
		tiamatRef->getController().delete_();
		spawnTiamat();
		spawnCalindi();
	}, 54500);
}

// Java DragonLordsRefugeInstance.java:271-273
void DragonLordsRefugeInstance::spawnTiamat() {
	spawn(219360, 452, 514, 432, int8_t{0});
}

// Java DragonLordsRefugeInstance.java:275-277
void DragonLordsRefugeInstance::spawnCalindi() {
	spawn(219359, 483.463f, 514.519f, 417.404f, int8_t{0});
}

// Java DragonLordsRefugeInstance.java:279-282
void DragonLordsRefugeInstance::onEnterInstance(Player& player) {
	raceId.compareAndSet(10, model::getRaceId(player.getRace()));
}

// Java DragonLordsRefugeInstance.java:284-288
void DragonLordsRefugeInstance::onCreatureDetected(Npc& detector, Creature& detected) {
	if (detector.getNpcId() == 219360 && dynamic_cast<Player*>(&detected) != nullptr && progress.compareAndSet(0, 1))
		handleDialogueEvent(detector);
}

// Java DragonLordsRefugeInstance.java:290-331
void DragonLordsRefugeInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	switch (npc.getNpcId()) {
		case 730625: {
			float distance = commons::utils::Rnd::get(250, 450) * 0.1f;
			double radian = commons::utils::Rnd::get(-45, 60) * std::numbers::pi / 180.0; // Math.toRadians
			float x = static_cast<float>(std::cos(radian) * distance);
			float y = static_cast<float>(std::sin(radian) * distance);
			TeleportService::teleportTo(*player, instance->getMapId(), 503.389f + x, 514.661f + y, 417.404f);
			runtime::Ptr<Npc> tiamat = getNpc(219360);
			if (tiamat != nullptr)
				tiamat->overrideNpcType(CreatureType::PEACE);
			break;
		}
		case 730630: // Exit
			TeleportService::moveToInstanceExit(*player, mapId, player->getRace());
			break;
		case 730673:
			TeleportService::teleportTo(*player, instance->getMapId(), 217.144f, 195.616f, 246.0712f);
			break;
		case 730674:
			TeleportService::teleportTo(*player, instance->getMapId(), 785.866f, 197.713f, 246.0712f);
			break;
		case 730675:
			TeleportService::teleportTo(*player, instance->getMapId(), 217.947f, 832.552f, 246.0712f);
			break;
		case 730676:
			TeleportService::teleportTo(*player, instance->getMapId(), 779.178f, 833.055f, 246.0712f);
			break;
		case 730633:
			TeleportService::teleportTo(*player, instance->getMapId(), 461.493f, 459.308f, 417.405f);
			break;
		case 730634:
			TeleportService::teleportTo(*player, instance->getMapId(), 546.0f, 458.934f, 417.405f);
			break;
		case 730635:
			TeleportService::teleportTo(*player, instance->getMapId(), 462.188f, 568.913f, 417.405f);
			break;
		case 730636:
			TeleportService::teleportTo(*player, instance->getMapId(), 545.773f, 569.225f, 417.405f);
			break;
	}
}

// Java DragonLordsRefugeInstance.java:333-337
void DragonLordsRefugeInstance::onInstanceDestroy() {
	if (runtime::FutureRef task = failTask.get(); task != nullptr && !task->isCancelled())
		task->cancel(true);
}

// Java DragonLordsRefugeInstance.java:339-347
bool DragonLordsRefugeInstance::onReviveEvent(Player& player) {
	services::player::PlayerReviveService::revive(player, 25, 25, true, 0);
	player.getGameStats()->updateStatsAndSpeedVisually();
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	std::vector<runtime::Ptr<WorldPosition>> locations = respawnLocations.snapshot();
	runtime::Ptr<WorldPosition> p = *commons::utils::Rnd::get(locations);
	TeleportService::teleportTo(player, *instance, p->getX(), p->getY(), p->getZ(), p->getHeading());
	return true;
}

} // namespace aion::gameserver::handlers::instance
