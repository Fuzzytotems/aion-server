#include "aion/gameserver/handlers/instance/DrakenspireDepthsInstance.h"

#include <string>

#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/RespawnService.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(DrakenspireDepthsInstance, 301390000);

/**
 * Java: the anonymous Runnable of onTwinsFightStart (DrakenspireDepthsInstance.java:232, $1), run every 10 s from 30 s by scheduleAtFixedRate
 * and kept in currentEventTask: its twinProgressCount is the run count.
 */
struct DrakenspireDepthsInstance_Runnable final : runtime::RefCounted {
	AION_MAKE_REF_FRIEND

	runtime::Field<int32_t> twinProgressCount{}; // int twinProgressCount (line 234)
	const runtime::Ref<DrakenspireDepthsInstance> drakenspireDepthsInstance; // captured this (line 240)

	static runtime::Ref<DrakenspireDepthsInstance_Runnable> create(DrakenspireDepthsInstance& handler) {
		return runtime::makeRef<DrakenspireDepthsInstance_Runnable>(handler);
	}

	// Java DrakenspireDepthsInstance.java:236-254
	void run() {
		DrakenspireDepthsInstance& self = *drakenspireDepthsInstance;
		twinProgressCount = twinProgressCount.get() + 1;
		switch (twinProgressCount.get()) {
			case 2:
				self.instance->setDoorState(380, true); // one of the best doors which closes if you set it open
				self.instance->setDoorState(374, true);
				self.instance->setDoorState(377, true);
				self.instance->setDoorState(379, true);
				self.instance->setDoorState(391, true);
				break;
			case 8:
				self.sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_04());
				break;
			case 9:
				self.sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_05(), 5000);
				break;
			case 10:
				self.cancelTasks({self.currentEventTask.get(), self.additionalEventTask.get()});
				self.onTwinsFail();
				self.sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_06());
				break;
		}
	}

protected:
	explicit DrakenspireDepthsInstance_Runnable(DrakenspireDepthsInstance& handler) : drakenspireDepthsInstance(handler) {}
	~DrakenspireDepthsInstance_Runnable() override = default;
};

/**
 * Java: the anonymous Runnable of scheduleWaveStarts (DrakenspireDepthsInstance.java:380, $2), run every 60 s from 6 s by
 * scheduleAtFixedRate and kept in currentEventTask: its waveCount is the run count.
 */
struct DrakenspireDepthsInstance_Runnable_2 final : runtime::RefCounted {
	AION_MAKE_REF_FRIEND

	runtime::Field<int32_t> waveCount{}; // int waveCount (line 382)
	const runtime::Ref<DrakenspireDepthsInstance> drakenspireDepthsInstance; // captured this (line 388)

	static runtime::Ref<DrakenspireDepthsInstance_Runnable_2> create(DrakenspireDepthsInstance& handler) {
		return runtime::makeRef<DrakenspireDepthsInstance_Runnable_2>(handler);
	}

	// Java DrakenspireDepthsInstance.java:384-416
	void run() {
		DrakenspireDepthsInstance& self = *drakenspireDepthsInstance;
		waveCount = waveCount.get() + 1;
		switch (waveCount.get()) {
			case 1:
				self.instance->setDoorState(267, true);
				self.instance->setDoorState(271, true);
				self.spawn(731581, 578.55f, 819.27f, 1609.42f, int8_t{0}, 84);
				self.spawn(731581, 690.49f, 822.41f, 1609.57f, int8_t{0}, 405);
				self.spawn(731581, 635.39f, 784.05f, 1596.72f, int8_t{0}, 548);
				break;
			case 2:
				if (self.difficulty.get() == DrakenspireDepthsInstance::HARD_MODE)
					self.sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_05());
				break;
			case 3:
				self.instance->setDoorState(7, true);
				self.instance->setDoorState(310, true);
				self.spawn(731581, 707.34f, 876.80f, 1603.69f, int8_t{0}, 398);
				self.spawn(731581, 570.70f, 877.51f, 1599.80f, int8_t{0}, 401);
				if (self.difficulty.get() == DrakenspireDepthsInstance::HARD_MODE)
					self.sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_06());
				break;
			case 4:
				self.sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_07());
				break;
			case 5:
				self.instance->setDoorState(210, true);
				self.instance->setDoorState(312, true);
				self.spawn(731581, 694.01f, 935.97f, 1618.09f, int8_t{0}, 399);
				self.spawn(731581, 572.94f, 940.04f, 1620.04f, int8_t{0}, 407);
				break;
		}
		if (waveCount.get() <= self.difficulty.get())
			self.scheduleCommanderWave(waveCount.get());
	}

protected:
	explicit DrakenspireDepthsInstance_Runnable_2(DrakenspireDepthsInstance& handler) : drakenspireDepthsInstance(handler) {}
	~DrakenspireDepthsInstance_Runnable_2() override = default;
};

// Java DrakenspireDepthsInstance.java:67-69
DrakenspireDepthsInstance::DrakenspireDepthsInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<DrakenspireDepthsInstance> DrakenspireDepthsInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<DrakenspireDepthsInstance>(instance);
}

// Java DrakenspireDepthsInstance.java:71-74
void DrakenspireDepthsInstance::onInstanceDestroy() {
	cancelTasks({currentEventTask.get(), additionalEventTask.get()});
}

// Java DrakenspireDepthsInstance.java:76-86
void DrakenspireDepthsInstance::onEnterInstance(Player& player) {
	if (race.compareAndSet(Race::NONE, player.getRace())) {
		spawn(race.get() == Race::ELYOS ? 209678 : 209743, 353.443f, 185.629f, 1686.1825f, int8_t{60});
		spawn(race.get() == Race::ELYOS ? 209679 : 209744, 353.443f, 179.259f, 1686.1825f, int8_t{60});
		spawn(race.get() == Race::ELYOS ? 209680 : 209745, 347.099f, 192.390f, 1686.1825f, int8_t{90});
		spawn(race.get() == Race::ELYOS ? 209680 : 209745, 351.295f, 192.437f, 1686.1825f, int8_t{90});
		spawn(race.get() == Race::ELYOS ? 209681 : 209746, 347.099f, 173.774f, 1686.1825f, int8_t{30});
		spawn(race.get() == Race::ELYOS ? 209681 : 209746, 351.295f, 173.821f, 1686.1825f, int8_t{30});
	}
}

// Java DrakenspireDepthsInstance.java:88-139
void DrakenspireDepthsInstance::onSpawn(VisibleObject& object) {
	if (Npc* npc = dynamic_cast<Npc*>(&object)) {
		switch (npc->getNpcId()) {
			case 236230: // Immortal Orissan
			case 236233:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_03(), 2500);
				break;
			case 236231: // Exhausted Orissan
			case 236234:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_05(), 2500);
				scheduleOrissansNextImmortality();
				break;
			case 702719: // Empyrean Lord's Siege Weapon
			case 702720:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_01(), 3000);
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_02(), 6000);
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_03(), 9000);
				break;
			case 236204: // Flamesquelch Destroyer
			case 236205: // Flamesquelch Burnsmark
			case 236206: // Flamesquelch Rangerblaze
			case 236216:
			case 236217:
			case 236218:
			case 236219:
			case 236220:
				startWalking(*npc, CreatureState::WALK_MODE);
				break;
			case 236243: // Commander Virtsha
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_08());
				break;
			case 236247: // Dragon Beritra
				// TODO: scheduleBeritrasDespawn();
				spawn(702699, 153.250f, 516.473f, 1751.0494f, int8_t{0}, 383); // Pillar remains
				break;
			case 855460:
			case 855461:
			case 855462:
			case 855463:
			case 855464: // Drakenspire Protector
			case 855465:
			case 855466:
			case 855467:
			case 855468:
			case 855469:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_GUARDIAN_01());
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_GUARDIAN_02(), 3000);
				break;
		}
	}
}

// Java DrakenspireDepthsInstance.java:141-214
void DrakenspireDepthsInstance::onDie(Npc& npc) {
	GeneralInstanceHandler::onDie(npc);
	int32_t npcId = npc.getNpcId();
	switch (npcId) {
		case 236225:
		case 236226: {
			runtime::Ptr<Npc> oppositeProtector = getNpc(npcId == 236225 ? 236226 : 236225);
			if (oppositeProtector == nullptr || oppositeProtector->isDead())
				onTwinsComplete();
			break;
		}
		case 236227: // Lava Protector
		case 236228: // Heatvent Protector
			if (getNpc(npcId == 236227 ? 855709 : 855708) != nullptr) {
				onTwinsComplete();
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_07());
			} else {
				spawn(npcId == 236227 ? 855708 : 855709, npc.getX(), npc.getY(), npc.getZ(), int8_t{0}); // Sphere
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_RESSURECT_02());
				setAdditionalEventTask(ThreadPoolManager::getInstance().schedule({this}, [this] { onTwinRespawn(); }, 15000));
			}
			break;
		case 236223: // Fetid Phantomscorch Chimera
		case 833012:
		case 833013:
		case 833015:
			return;
		case 236224: // Rapacious Kadena
			instance->setDoorState(375, true);
			break;
		case 236661: // Rapacious Kadena
			instance->setDoorState(376, true);
			break;
		case 236662: // Rapacious Kadena
			instance->setDoorState(378, true);
			break;
		case 236231: // Exhausted Orissan
		case 236234:
			onOrissanComplete();
			deleteAliveNpcs({855607, 855608, 855699, 855700});
			break;
		case 236239: // Wave Commanders
		case 236240:
		case 236241:
		case 236242:
		case 236243:
			if (eventCounter.incrementAndGet() >= difficulty.get()) // difficulty is equivalent to the necessary kills
				onWaveEventComplete(true);
			break;
		case 236244: // Beritra Easy Mode
		case 236245: { // Beritra Normal Mode
			runtime::Ptr<WorldPosition> pos = npc.getPosition();
			spawn(npcId == 236244 ? 833012 : 833013, pos->getX(), pos->getY(), pos->getZ(), pos->getHeading());
			spawnExit();
			break;
		}
		case 236246: // Beritra Hard Mode
		case 236247: // Beritra Dragon
			// TODO: spawn exit and NPCs
			spawnExit();
			break;
		case 236248:
		case 236249:
			if (waveSurvivors.decrementAndGet() == 0)
				onWaveEventComplete(false);
			break;
		// exploding flame 856459 skill use
		case 731580:
			services::RespawnService::scheduleDecayTask(npc, 4000);
			switch (npc.getSpawn()->getStaticId()) {
				case 332:
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_09());
					break;
				case 522:
				case 603:
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_08());
					break;
			}
	}
}

// Java DrakenspireDepthsInstance.java:216-229
void DrakenspireDepthsInstance::onAggro(Npc& npc) {
	switch (npc.getNpcId()) {
		case 236227: // Lava Protector
		case 236228: // Heatvent Protector
			if (isStageActive.compareAndSet(false, true)) {
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_01());
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_02(), 2500);
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_03(), 5000);
				onTwinsFightStart();
			}
			break;
	}
}

// Java DrakenspireDepthsInstance.java:231-257
void DrakenspireDepthsInstance::onTwinsFightStart() {
	runtime::Ref<DrakenspireDepthsInstance_Runnable> runnable = DrakenspireDepthsInstance_Runnable::create(*this);
	setCurrentEventTask(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [runnable] { runnable->run(); }, 30000, 10 * 1000));
}

// Java DrakenspireDepthsInstance.java:259-266
void DrakenspireDepthsInstance::onTwinRespawn() {
	if (getNpc(855708) != nullptr)
		spawn(236227, 531.0885f, 212.43806f, 1683.4116f, int8_t{60});
	if (getNpc(855709) != nullptr)
		spawn(236228, 530.8584f, 151.8681f, 1683.4116f, int8_t{60});
	deleteAliveNpcs({855708, 855709});
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_RESSURECT_03());
}

// Java DrakenspireDepthsInstance.java:268-289
void DrakenspireDepthsInstance::onTwinsComplete() {
	cancelTasks({currentEventTask.get(), additionalEventTask.get()});
	deleteAliveNpcs({855708, 855709});

	ThreadPoolManager::getInstance().schedule({this}, [this] {
		sp(race.get() == Race::ELYOS ? 209690 : 209755, 543.35f, 149.81f, 1681.82f, int8_t{0}, "301390000_NPCPathFunction_Npc_Path03", 4000);
		sp(race.get() == Race::ELYOS ? 209690 : 209755, 543.45f, 208.97f, 1681.82f, int8_t{0}, "301390000_NPCPathFunction_Npc_Path01", 4000);
		sp(race.get() == Race::ELYOS ? 209693 : 209758, 543.43f, 155.08f, 1681.82f, int8_t{0}, "301390000_NPCPathFunction_Npc_Path04", 4000);
		sp(race.get() == Race::ELYOS ? 209693 : 209758, 543.43f, 214.24f, 1681.82f, int8_t{0}, "301390000_NPCPathFunction_Npc_Path02", 4000);
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			spawn(race.get() == Race::ELYOS ? 209695 : 209760, 582.61f, 183.52f, 1683.73f, int8_t{0});
			spawn(race.get() == Race::ELYOS ? 209694 : 209759, 582.55f, 178.02f, 1683.73f, int8_t{0});
		}, 16000);
	}, 2000);

	if (difficulty.get() == HARD_MODE)
		spawn(236232, 812.0238f, 568.3602f, 1701.045f, int8_t{92}); // HM Orissan
	else
		spawn(236229, 812.0238f, 568.3602f, 1701.045f, int8_t{92}); // NM Orissan

	isStageActive.set(false);
}

// Java DrakenspireDepthsInstance.java:291-297
void DrakenspireDepthsInstance::onTwinsFail() {
	if (difficulty.compareAndSet(HARD_MODE, NORMAL_MODE)) {
		deleteAliveNpcs({236227, 236228, 855708, 855709});
		spawn(236225, 531.0885f, 212.4381f, 1683.412f, int8_t{60}); // Fountless Lava Protector
		spawn(236226, 530.8584f, 151.8681f, 1683.412f, int8_t{60}); // Fountless Heatvent Protector
	}
}

// Java DrakenspireDepthsInstance.java:304-335
void DrakenspireDepthsInstance::scheduleOrissansNextImmortality() {
	int32_t delay;
	switch (eventCounter.incrementAndGet()) {
		case 1:
			delay = 120000;
			break;
		case 2:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_06(), 5000);
			delay = 228000;
			break;
		case 3: {
			int32_t prevDiff = difficulty.get();
			switch (difficulty.getAndSet(prevDiff - 2)) {
				case NORMAL_MODE:
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_07(), 5000);
					break;
				case EASY_MODE:
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_08(), 5000);
					break;
			}
			delay = 240000; // Let's give them a few seconds more
			break;
		}
		default:
			// Maybe retail has lowered the difficulty to easy mode at this point (if normal mode was present)
			switch (difficulty.get()) {
				case NORMAL_MODE:
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_09(), 5000);
					break;
				case EASY_MODE:
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_10(), 5000);
					break;
			}
			return;
	}

	setCurrentEventTask(ThreadPoolManager::getInstance().schedule({this}, [this] {
		runtime::Ptr<Npc> orissan = getNpc(difficulty.get() == HARD_MODE ? 236234 : 236231);
		if (orissan != nullptr) // should not happen
			SkillEngine::getInstance().getSkill(*orissan, 21635, 1, runtime::Ptr<VisibleObject>(orissan))->useSkill(); // Summon Crystal
	}, delay));
}

// Java DrakenspireDepthsInstance.java:337-352
void DrakenspireDepthsInstance::onOrissanComplete() {
	cancelTasks({currentEventTask.get()});
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		spawn(race.get() == Race::ELYOS ? 209705 : 209770, 816.18f, 517.78f, 1707.41f, int8_t{33});
		spawn(race.get() == Race::ELYOS ? 209705 : 209770, 820.82f, 517.82f, 1707.41f, int8_t{33});
		spawn(race.get() == Race::ELYOS ? 209704 : 209769, 816.00f, 521.35f, 1706.88f, int8_t{33});
		spawn(race.get() == Race::ELYOS ? 209704 : 209769, 820.48f, 521.49f, 1706.88f, int8_t{33});

		spawn(race.get() == Race::ELYOS ? 209711 : 209776, 811.00f, 587.83f, 1701.045f, int8_t{32});
		spawn(race.get() == Race::ELYOS ? 209709 : 209774, 807.50f, 583.54f, 1701.045f, int8_t{32});
		spawn(race.get() == Race::ELYOS ? 209710 : 209775, 814.85f, 583.56f, 1701.045f, int8_t{32});
		spawn(race.get() == Race::ELYOS ? 209708 : 209773, 811.11f, 582.62f, 1701.045f, int8_t{32});
	}, 4000);
	eventCounter.set(0);
	spawn(206405, 639.50f, 866.50f, 1600.88f, int8_t{0});
}

// Java DrakenspireDepthsInstance.java:354-377
void DrakenspireDepthsInstance::onWaveEventStart() {
	sp(race.get() == Race::ELYOS ? 209720 : 209785, 635.53f, 886.83f, 1600.72f, int8_t{30}, "301390000_NPCPathFunction_Npc_Path06",
		CreatureState::WALK_MODE, 7000);
	spawn(race.get() == Race::ELYOS ? 209731 : 209796, 639.05f, 895.86f, 1600.41f, int8_t{30});
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		sp(race.get() == Race::ELYOS ? 209722 : 209787, 637.75f, 847.98f, 1599.94f, int8_t{30}, "301390000_NPCPathFunction_Npc_Path07", 2000);
		sp(race.get() == Race::ELYOS ? 209722 : 209787, 637.75f, 847.98f, 1599.94f, int8_t{30}, "301390000_NPCPathFunction_Npc_Path08", 2000);
		sp(race.get() == Race::ELYOS ? 209722 : 209787, 637.75f, 847.98f, 1599.94f, int8_t{30}, "301390000_NPCPathFunction_Npc_Path09", 2000);
		sp(race.get() == Race::ELYOS ? 209722 : 209787, 637.75f, 847.98f, 1599.94f, int8_t{30}, "301390000_NPCPathFunction_Npc_Path10", 2000);
		sp(race.get() == Race::ELYOS ? 209722 : 209787, 633.95f, 847.58f, 1599.87f, int8_t{30}, "301390000_NPCPathFunction_Npc_Path13", 2000);
		sp(race.get() == Race::ELYOS ? 209722 : 209787, 633.95f, 847.58f, 1599.87f, int8_t{30}, "301390000_NPCPathFunction_Npc_Path14", 2000);
		sp(race.get() == Race::ELYOS ? 209722 : 209787, 633.95f, 847.58f, 1599.87f, int8_t{30}, "301390000_NPCPathFunction_Npc_Path15", 2000);
		sp(race.get() == Race::ELYOS ? 209722 : 209787, 633.95f, 847.58f, 1599.87f, int8_t{30}, "301390000_NPCPathFunction_Npc_Path16", 2000);
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			deleteAliveNpcs({race.get() == Race::ELYOS ? 209720 : 209785});
			spawn(race.get() == Race::ELYOS ? 209723 : 209788, 636.09f, 879.98f, 1600.82f, int8_t{33}); // Main Npc
			spawn(race.get() == Race::ELYOS ? 702719 : 702720, 635.68f, 883.03f, 1603.91f, int8_t{29}); // Siege Weapon
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_01(), 3000);
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_02(), 6000);
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_03(), 9000);
			scheduleWaveStarts();
		}, 12000);
	}, 18000);
}

// Java DrakenspireDepthsInstance.java:379-419
void DrakenspireDepthsInstance::scheduleWaveStarts() {
	runtime::Ref<DrakenspireDepthsInstance_Runnable_2> runnable = DrakenspireDepthsInstance_Runnable_2::create(*this);
	setCurrentEventTask(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [runnable] { runnable->run(); }, 6000, 60000));
}

// Java DrakenspireDepthsInstance.java:421-433
void DrakenspireDepthsInstance::scheduleCommanderWave(int32_t waveCount) {
	setAdditionalEventTask(ThreadPoolManager::getInstance().schedule({this}, [this, waveCount] {
		sp(commons::utils::Rnd::get(236216, 236220), 634.80f, 790.95f, 1596.80f, int8_t{30}, "301390000_Wave_Commander_Left", 0);
		sp(commons::utils::Rnd::get(236216, 236220), 634.80f, 790.95f, 1596.80f, int8_t{30}, "301390000_Wave_Commander_Right", 0);
		if (waveCount >= 3)
			sp(commons::utils::Rnd::get(236216, 236220), 634.80f, 790.95f, 1596.80f, int8_t{30}, "301390000_Wave_Commander_Middle", 0);
		if (waveCount >= 4) {
			sp(commons::utils::Rnd::get(236216, 236220), 634.80f, 790.95f, 1596.80f, int8_t{30}, "301390000_Wave_Commander_Left", 4000);
			sp(commons::utils::Rnd::get(236216, 236220), 634.80f, 790.95f, 1596.80f, int8_t{30}, "301390000_Wave_Commander_Right", 4000);
		}
		sp(236243 - difficulty.get() + waveCount, 634.80f, 790.95f, 1596.80f, int8_t{30}, "301390000_Wave_Commander_Middle", 3000); // Boss
	}, 28000));
}

// Java DrakenspireDepthsInstance.java:435-457
void DrakenspireDepthsInstance::onWaveEventComplete(bool isSuccessful) {
	cancelTasks({currentEventTask.get(), additionalEventTask.get()});
	deleteAliveNpcs({236204, 236205, 236206, 236216, 236217, 236218, 236219, 236220, 236239, 236240, 236241, 236242, 236243, 731581});
	if (isSuccessful) {
		SkillEngine::getInstance()
			.getSkill(*getNpc(race.get() == Race::ELYOS ? 702719 : 702720), 20838, 1, runtime::Ptr<VisibleObject>(getNpc(731580)))
			->useSkill();
		switch (waveSurvivors.get()) {
			case 8:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_BONUS_04());
				break;
			case 7:
			case 6:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_BONUS_03());
				break;
			case 5:
			case 4:
			case 3:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_BONUS_02());
				break;
			case 2:
			case 1:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_BONUS_01());
				break;
		}
		switch (difficulty.get()) {
			// case 5 -> spawn(236246, 152f, 518f, 1749.6f, (byte) 55); TODO: Implementation
			case HARD_MODE:
			case NORMAL_MODE:
				spawn(236245, 152.38f, 518.68f, 1749.6f, int8_t{55});
				break;
			case EASY_MODE:
				spawn(236244, 152.38f, 518.68f, 1749.6f, int8_t{55});
				break;
		}

	} else {
		deleteAliveNpcs({702719, 702720});
		// TODO: find sys msg for fail
	}
}

// Java DrakenspireDepthsInstance.java:459-486
void DrakenspireDepthsInstance::spawnExit() {
	Race r = race.get();
	switch (difficulty.get()) {
		case FAILED:
			spawn(r == Race::ELYOS ? 209741 : 209806, 155.390f, 516.622f, 1750.0f, int8_t{0}); // Masionel | Parsia
			spawn(r == Race::ELYOS ? 209742 : 209807, 155.361f, 520.999f, 1750.0f, int8_t{0}); // Detachment Entry Soldier
			break;
		case EASY_MODE:
			spawn(r == Race::ELYOS ? 209740 : 209805, 155.390f, 516.622f, 1750.0f, int8_t{0}); // Masionel | Parsia
			spawn(r == Race::ELYOS ? 209742 : 209807, 155.361f, 520.999f, 1750.0f, int8_t{0}); // Detachment Entry Soldier
			break;
		case NORMAL_MODE:
			spawn(r == Race::ELYOS ? 209739 : 209804, 155.390f, 516.622f, 1750.0f, int8_t{0}); // Masionel | Parsia
			spawn(r == Race::ELYOS ? 209742 : 209807, 155.361f, 520.999f, 1750.0f, int8_t{0}); // Detachment Entry Soldier
			break;
		case HARD_MODE:
			spawn(r == Race::ELYOS ? 209738 : 209803, 140.656f, 518.922f, 1750.0f, int8_t{0}); // Masionel | Parsia
			spawn(r == Race::ELYOS ? 209742 : 209807, 144.452f, 516.218f, 1749.4784f, int8_t{0}); // Detachment Entry Soldier
			spawn(r == Race::ELYOS ? 209742 : 209807, 144.433f, 521.472f, 1749.4768f, int8_t{0}); // Detachment Entry Soldier
			break;
	}
	// Exits
	spawn(731548, 320.005f, 183.281f, 1687.2552f, int8_t{60}); // Start Exit
	spawn(731548, 545.194f, 153.283f, 1681.8224f, int8_t{60}); // Protector Exit
	spawn(731548, 545.325f, 212.691f, 1681.8224f, int8_t{60}); // Protector Exit
	spawn(731548, 810.652f, 591.986f, 1701.0449f, int8_t{90}); // Orissan Exit
	spawn(731548, 137.524f, 518.576f, 1749.4153f, int8_t{0}); // Beritra Exit
}

// Java DrakenspireDepthsInstance.java:488-498
void DrakenspireDepthsInstance::onCreatureDetected(Npc& detector, Creature& detected) {
	if (dynamic_cast<Player*>(&detected) == nullptr)
		return;
	if (detector.getNpcId() == 206405) {
		if (isStageActive.compareAndSet(false, true)) {
			onWaveEventStart();
			detector.getController().delete_();
		}
	}
}

// Java DrakenspireDepthsInstance.java:500-510
void DrakenspireDepthsInstance::onStartEffect(runtime::Ptr<Effect> effect) {
	switch (effect->getSkillId()) {
		case 21635: // Summon Crystal
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_01());
			break;
		case 21885: // Weaken Ascension Domination
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_04());
			break;
		case 21610: // Dark Affinity
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_VRITRA_HUMAN_01());
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_VRITRA_HUMAN_02(), 3000);
			break;
	}
}

// Java DrakenspireDepthsInstance.java:512-529
void DrakenspireDepthsInstance::onEndEffect(Effect& effect) {
	switch (effect.getSkillId()) {
		case 21635:
		case 21885: { // Weaken Ascension Domination
			int32_t nextId;
			if (effect.getSkillId() == 21635)
				nextId = difficulty.get() == HARD_MODE ? 236233 : 236230;
			else
				nextId = difficulty.get() == HARD_MODE ? 236234 : 236231;
			runtime::Ptr<WorldPosition> pos = effect.getEffected()->getPosition();
			effect.getEffected()->getController().delete_();
			spawn(nextId, pos->getX(), pos->getY(), pos->getZ(), pos->getHeading());
			break;
		}
		case 21618: // Dragon Lord's Authority
			// TODO: sendMsg(SM_SYSTEM_MESSAGE.STR_MSG_IDSEAL_VRITRA_HUMAN_03()); => only if it ends naturally
			break;
	}
}

// Java DrakenspireDepthsInstance.java:531-534
void DrakenspireDepthsInstance::leaveInstance(Player& player) {
	TeleportService::moveToInstanceExit(player, mapId, player.getRace());
}

// Java DrakenspireDepthsInstance.java:536-543
bool DrakenspireDepthsInstance::onReviveEvent(Player& player) {
	services::player::PlayerReviveService::revive(player, 25, 25, false, 0);
	player.getGameStats()->updateStatsAndSpeedVisually();
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	TeleportService::teleportTo(player, *instance, 361.706f, 182.503f, 1684.290f, int8_t{0});
	return true;
}

// Java DrakenspireDepthsInstance.java:545-548
void DrakenspireDepthsInstance::setCurrentEventTask(runtime::FutureRef task) {
	SYNCHRONIZED(monitor) {
		cancelTasks({currentEventTask.get()}); // lockdep: Field::get, not Future::get - the field read is non-blocking
		currentEventTask.set(std::move(task));
	}
}

// Java DrakenspireDepthsInstance.java:550-553
void DrakenspireDepthsInstance::setAdditionalEventTask(runtime::FutureRef task) {
	SYNCHRONIZED(monitor) {
		cancelTasks({additionalEventTask.get()}); // lockdep: Field::get, not Future::get - the field read is non-blocking
		additionalEventTask.set(std::move(task));
	}
}

// Java DrakenspireDepthsInstance.java:555-559
void DrakenspireDepthsInstance::cancelTasks(std::initializer_list<runtime::FutureRef> tasks) {
	SYNCHRONIZED(monitor) {
		for (const runtime::FutureRef& task : tasks)
			if (task != nullptr && !task->isDone())
				task->cancel(true);
	}
}

// Java DrakenspireDepthsInstance.java:561-563
void DrakenspireDepthsInstance::sp(int32_t id, float x, float y, float z, int8_t h, std::string_view walkerId, int32_t delay) {
	sp(id, x, y, z, h, walkerId, CreatureState::ACTIVE, delay);
}

// Java DrakenspireDepthsInstance.java:565-571
void DrakenspireDepthsInstance::sp(int32_t id, float x, float y, float z, int8_t h, std::string_view walkerId, CreatureState state, int32_t delay) {
	ThreadPoolManager::getInstance().schedule({this}, [this, id, x, y, z, h, walker = std::string(walkerId), state] {
		runtime::Ptr<Npc> npc = runtime::cast<Npc>(spawn(id, x, y, z, h));
		npc->getSpawn()->setWalkerId(std::string_view(walker));
		startWalking(*npc, state);
	}, delay);
}

// Java DrakenspireDepthsInstance.java:573-584
void DrakenspireDepthsInstance::startWalking(Npc& npc, CreatureState state) {
	runtime::Ref<Npc> walker(npc); // Java's `if (npc == null) return;` (inside the task) cannot hold: callers pass a spawned npc
	ThreadPoolManager::getInstance().schedule({this}, [walker, state] {
		gameserver::ai::manager::WalkManager::startWalking(dynamic_cast<gameserver::ai::NpcAI&>(walker->getAi()));
		if (state == CreatureState::ACTIVE) {
			walker->unsetState(CreatureState::WALK_MODE);
			walker->setState(state);
			PacketSendUtility::broadcastPacket(*walker, network::aion::serverpackets::SM_EMOTION(*walker, EmotionType::CHANGE_SPEED));
		}
	}, 2000);
}

// Java DrakenspireDepthsInstance.java:586-592
bool DrakenspireDepthsInstance::isBoss(Npc& npc) {
	switch (npc.getNpcId()) {
		case 236227:
		case 236228:
		case 236231:
		case 236234:
		case 236244:
		case 236245:
		case 236247:
			return true;
		default:
			return false;
	}
}

} // namespace aion::gameserver::handlers::instance
