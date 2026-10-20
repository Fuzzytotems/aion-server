#include "aion/gameserver/handlers/instance/AnguishedDragonLordsRefugeInstance.h"

#include <algorithm>
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

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(AnguishedDragonLordsRefugeInstance, 300630000);

// Java AnguishedDragonLordsRefugeInstance.java:26-28
AnguishedDragonLordsRefugeInstance::AnguishedDragonLordsRefugeInstance(WorldMapInstance& instance) : DragonLordsRefugeInstance(instance) {
}

runtime::Ref<AnguishedDragonLordsRefugeInstance> AnguishedDragonLordsRefugeInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<AnguishedDragonLordsRefugeInstance>(instance);
}

// Java AnguishedDragonLordsRefugeInstance.java:30-87
void AnguishedDragonLordsRefugeInstance::onSpawn(VisibleObject& object) {
	if (Npc* npc = dynamic_cast<Npc*>(&object)) {
		switch (npc->getNpcId()) {
			case 236276: // Tiamat Dragon
				ThreadPoolManager::getInstance().schedule({this}, [this] {
					spawn(730673, 459.548f, 456.849f, 417.405f, int8_t{21});
					spawn(730674, 547.909f, 456.568f, 417.405f, int8_t{45});
					spawn(730675, 460.082f, 571.978f, 417.405f, int8_t{98});
					spawn(730676, 547.822f, 571.876f, 417.405f, int8_t{74});
				}, 60000);
				break;
			case 236277: // Tiamat Weakened Dragon
				sendMsg(SM_SYSTEM_MESSAGE::IDTIAMAT_TIAMAT_COUNTDOWN_START(), 2000);
				PacketSendUtility::broadcastToMap(*instance, SM_QUEST_ACTION(0, 1800));
				failTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
					runtime::Ptr<Npc> tiamat = getNpc(236277);
					if (tiamat != nullptr && !tiamat->isDead()) {
						endInstance({730625, 730633, 730634, 730635, 730636});
						sendMsg(SM_SYSTEM_MESSAGE::IDTIAMAT_TIAMAT_COUNTDOWN_OVER());
					}
				}, 1800000)); // 30'
				break;
			case 856020: // Kaisinel 1st Phase
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_2PHASE_START_LIGHT(), 20000);
				break;
			case 856021: // Kaisinel 2nd Phase
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_KAISINEL_2PHASE_DEADLYATK(), 3000);
				break;
			case 856022: // Kaisinel 3rd Phase
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_KAISINEL_2PHASE_GROGGY(), 15000);
				break;
			case 856023: // Marchutan 1st Phase
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_2PHASE_START_DARK(), 20000);
				break;
			case 856024: // Marchutan 2nd Phase
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_MARCHUTAN_2PHASE_DEADLYATK(), 3000);
				break;
			case 856025: // Marchutan 3rd Phase
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_MARCHUTAN_2PHASE_GROGGY(), 15000);
				break;
			case 856026:
				sendMsg(SM_SYSTEM_MESSAGE::STR_IDTIAMAT_TIAMAT_SPAWN_BLACKHOLE(), 2500);
				break;
			case 730673:
				ThreadPoolManager::getInstance().schedule({this}, [this] {
					spawn(856483, 463.0f, 568.0f, 417.405f, int8_t{105});
					spawn(856484, 545.0f, 568.0f, 417.405f, int8_t{78});
					spawn(856485, 545.0f, 461.0f, 417.405f, int8_t{46});
					spawn(856486, 463.0f, 461.0f, 417.405f, int8_t{17});
				}, 10000);
				break;
			case 730695: // Surkana
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_KALYNDI_SURKANA_SPAWN(), 2500);
				break;
		}
	}
}

// Java AnguishedDragonLordsRefugeInstance.java:89-115
void AnguishedDragonLordsRefugeInstance::onStartEffect(runtime::Ptr<Effect> effect) {
	switch (effect->getSkillId()) {
		case 20918:
			PacketSendUtility::broadcastMessage(getNpc(800341), 1500612);
			break;
		case 20920:
			if (progress.compareAndSet(1, 2)) {
				ThreadPoolManager::getInstance().schedule({this}, [this] { spawn(856020 + raceId.get() * 3, 504.0f, 515.0f, 417.405f, int8_t{60}); }, 6000);
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

// Java AnguishedDragonLordsRefugeInstance.java:117-137
void AnguishedDragonLordsRefugeInstance::onEndEffect(Effect& effect) {
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
			if (runtime::cast<Npc>(effect.getEffector())->getNpcId() == 236276)
				endInstance({730625, 730633, 730634, 730635, 730636});
			break;
	}
}

// Java AnguishedDragonLordsRefugeInstance.java:139-145
void AnguishedDragonLordsRefugeInstance::endInstance(std::initializer_list<int32_t> despawnExceptions) {
	const std::vector<int32_t> exceptions(despawnExceptions);
	instance->forEachNpc([&exceptions](Npc& npc) {
		if (std::ranges::find(exceptions, npc.getNpcId()) == exceptions.end())
			npc.getController().delete_();
	});
	spawn(833482, 548.18683f, 514.54523f, 420.0f, int8_t{0}, 23);
}

// Java AnguishedDragonLordsRefugeInstance.java:147-205
void AnguishedDragonLordsRefugeInstance::onDie(Npc& npc) {
	DragonLordsRefugeInstance::onDie(npc);
	switch (npc.getNpcId()) {
		case 236274: // Calindi Flamelord
			deleteAliveNpcs({730695, 730696}); // Surkana
			ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::useSkill(getNpc(236275)->getAi(), 20919); }, 4000);
			ThreadPoolManager::getInstance().schedule({this}, [this] { deleteAliveNpcs({730694}); }, 6000); // Aetheric field
			break;
		case 236276: // Tiamat Dragon - killed by Empyrean Lord
			getNpc(730699)->getController().die(); // Animates roof destruction
			getNpc(730700)->getController().die();
			spawn(283134, 451.97f, 514.55f, 417.40436f, int8_t{0});
			spawn(236277, 451.97f, 514.55f, 417.40436f, int8_t{0});
			spawn(730704, 437.541f, 513.487f, 415.824f, int8_t{0}, 17); // Collapsed Debris impaling Tiamat
			break;
		case 236277:
			failTask.get()->cancel(true);
			PacketSendUtility::broadcastToMap(*instance, SM_QUEST_ACTION(0, 0));
			// TODO: play movie
			// Java bug (J-P6I13-2): the normal refuge's ids 219490, 219493, 219362 are spared
			endInstance({219490, 219493, 219362, 730625, 730633, 730634, 730635, 730636});
			spawn(702729, 480.0f, 514.0f, 417.405f, int8_t{0}); // Treasure Chest
			spawn(800430, 506.8f, 511.4f, 417.405f, int8_t{60}); // Kahrun
			spawn(800350 + raceId.get() * 6, 506.7f, 518.4f, 417.405f, int8_t{60}); // human Kaisinel/Marchutan
			spawn(800464, 544.964f, 517.898f, 417.405f, int8_t{113});
			spawn(800465, 545.605f, 510.325f, 417.405f, int8_t{17});
			break;
		case 236278: // Incarnations
		case 236279:
		case 236280:
		case 236281:
			if (incarnationKills.incrementAndGet() == 4)
				handlePhaseProgress();
			SkillEngine::getInstance().applyEffectDirectly(npc.getNpcId() - 215299, npc, *getNpc(236276)); // 20979 - 20982
			break;
		case 856483: // Balaur Spiritualist
		case 856484:
		case 856485:
		case 856486: {
			runtime::Ptr<Npc> empyreanLord = getNpc(856020 + raceId.get() * 3);
			if (empyreanLord != nullptr)
				SkillEngine::getInstance().applyEffectDirectly(npc.getNpcId() - 835490, npc, *empyreanLord); // 20993 - 20996
			npc.getController().delete_();
			break;
		}
		case 856020: // Kaisinel 1st Phase
		case 856023: // Marchutan 1st Phase
			if (incarnationKills.get() == 4) // in rare cases the empyrean lord dies while the instance have to wait for progress
				return;
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_DEADLYHOWLING());
			// the correction of 2026-10-19 (was J-P6I13-1): the anguished incarnations; Java looped over 283163-283166, the normal refuge's
			// spiritualists
			for (int32_t i = 236278; i <= 236281; i++) {
				runtime::Ptr<Npc> incarnation = getNpc(i);
				if (incarnation != nullptr)
					AIActions::useSkill(incarnation->getAi(), 20983);
			}
			AIActions::useSkill(getNpc(236276)->getAi(), 20983);
			ThreadPoolManager::getInstance().schedule({this}, [this] {
				endInstance({730625, 730633, 730634, 730635, 730636, 730699, 730700});
			}, 7000);
			break;
	}
}

// Java AnguishedDragonLordsRefugeInstance.java:214-225
void AnguishedDragonLordsRefugeInstance::handlePhaseProgress() {
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_2PHASE_CLOSE_ALL());

	int32_t empyreanLordId = 856020 + raceId.get() * 3;
	getNpc(236276)->getEffectController()->removeEffect(20984); // Dispel Unbreakable Wing

	// schedule spawn of empyrean lords for final attack to tiamat before getting exhausted
	ThreadPoolManager::getInstance().schedule({this}, [this, empyreanLordId] {
		deleteAliveNpcs({empyreanLordId});
		spawn(856021 + raceId.get() * 3, 516.285f, 514.84f, 417.405f, int8_t{60});
	}, 30000);
}

// Java AnguishedDragonLordsRefugeInstance.java:207-212
void AnguishedDragonLordsRefugeInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	DragonLordsRefugeInstance::handleUseItemFinish(player, npc);
	if (npc.getNpcId() == 833482) // Exit
		TeleportService::moveToInstanceExit(*player, mapId, player->getRace());
}

// Java AnguishedDragonLordsRefugeInstance.java:227-230
void AnguishedDragonLordsRefugeInstance::spawnTiamat() {
	spawn(236275, 452, 514, 432, int8_t{0});
}

// Java AnguishedDragonLordsRefugeInstance.java:232-235
void AnguishedDragonLordsRefugeInstance::spawnCalindi() {
	spawn(236274, 483.463f, 514.519f, 417.404f, int8_t{0});
}

} // namespace aion::gameserver::handlers::instance
