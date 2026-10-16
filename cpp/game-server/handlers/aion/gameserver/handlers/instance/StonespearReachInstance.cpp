#include "aion/gameserver/handlers/instance/StonespearReachInstance.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/instance/instancescore/LegionDominionScore.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/instanceinfo/LegionDominionScoreWriter.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/LegionDominionService.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/services/abyss/GloryPointsService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(StonespearReachInstance, 301500000);

// Java StonespearReachInstance.java:53-55 (and the field initializers, :44-51)
StonespearReachInstance::StonespearReachInstance(WorldMapInstance& instance)
	: GeneralInstanceHandler(instance) {
}

runtime::Ref<StonespearReachInstance> StonespearReachInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<StonespearReachInstance>(instance);
}

int32_t StonespearReachInstance::rndNpcId(int32_t base) {
	return base + commons::utils::Rnd::get(0, 8);
}

void StonespearReachInstance::addTask(std::function<void()> runnable, int32_t delay) {
	tasks.add(ThreadPoolManager::getInstance().schedule({this}, std::move(runnable), delay));
}

// Java StonespearReachInstance.java:57-73
void StonespearReachInstance::onInstanceCreate() {
	reward = LegionDominionScore::create();
	reward.get()->setInstanceProgressionType(InstanceProgressionType::PREPARING);
	spawn(833284, 231.14f, 264.399f, 96.23f, int8_t{1}, 14);
	addWorldPoints();
	if (timer.get() == nullptr) {
		startTime = commons::utils::currentTimeMillis();
		timer.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
			startTime = commons::utils::currentTimeMillis();
			reward.get()->setInstanceProgressionType(InstanceProgressionType::START_PROGRESS);
			sendPacket(std::nullopt, 0);
			startInstance();
			startFailTask();
		}, 180000)); // 3min
	}
}

// Java StonespearReachInstance.java:75-98
void StonespearReachInstance::onEnterInstance(Player& player) {
	if (!allowedPlayers.contains(player.getObjectId())) {
		if (allowedPlayers.size() >= 12) {
			TeleportService::teleportTo(player, *instance, 152, 264, 103);
		} else {
			allowedPlayers.add(player.getObjectId());
		}
	}

	if (deadPlayers.contains(player.getObjectId())) {
		TeleportService::teleportTo(player, *instance, 152, 264, 103);
	}

	if (reward.get() != nullptr && !reward.get()->isRewarded()) {
		sendPacket(std::nullopt, 0);
	}
	if (instanceLegion.get() == nullptr) {
		instanceLegion = runtime::Ref<Legion>(player.getLegion());
	}
	if (!instanceRace.get()) {
		instanceRace = player.getRace();
	}
}

// Java StonespearReachInstance.java:100-206
void StonespearReachInstance::onDie(Npc& npc) {
	GeneralInstanceHandler::onDie(npc);
	switch (npc.getNpcId()) {
		// trash mobs
		case 855765:
		case 855766:
		case 855767:
		case 855768:
		case 855769:
		case 855770:
		case 855771:
		case 855772:
		case 855773:
			addPoints(npc, 100);
			break;
		case 855788:
		case 855789:
		case 855790:
		case 855791:
		case 855792:
		case 855793:
		case 855794:
		case 855795:
		case 855796:
			addPoints(npc, 200);
			break;
		case 855811:
		case 855812:
		case 855813:
		case 855814:
		case 855815:
		case 855816:
		case 855817:
		case 855818:
		case 855819:
			addPoints(npc, 300);
			break;
		case 855834:
		case 855835:
		case 855836:
		case 855837:
		case 855838:
		case 855839:
		case 855840:
		case 855841:
		case 855842:
			addPoints(npc, 400);
			break;
		// aetherfield
		case 855764:
			aethericKills = aethericKills.get() + 1;
			addPoints(npc, 500);
			break;
		case 855787:
			aethericKills = aethericKills.get() + 1;
			addPoints(npc, 1000);
			break;
		case 856303: // kebbit
			addPoints(npc, 1500);
			break;
		case 855810:
			aethericKills = aethericKills.get() + 1;
			addPoints(npc, 1500);
			break;
		case 855833:
			aethericKills = aethericKills.get() + 1;
			addPoints(npc, 2000);
			break;
		// bosses
		case 856305: // clown
		case 855774: // hamerun
		case 855775: // kromede
		case 855776: // kalliga
			addPoints(npc, 12000);
			break;
		case 855797: // bakarma
		case 855798: // triroan
		case 855799: // lanmark
			addPoints(npc, 21000);
			break;
		case 855820: // calindi
		case 855821: // tahabata
		case 855822: // stormwing
			addPoints(npc, 30000);
			break;
		case 855843: // guardian general
			addPoints(npc, 42000);
			checkRank(reward.get()->getPoints(), true);
			break;
		// guardian stone
		// elyos
		case 855763:
		case 855786:
		case 855809:
		case 855832:
			// asmodian
		case 856466:
		case 856467:
		case 856468:
		case 856469:
			checkRank(reward.get()->getPoints(), false);
			break;
		default:
			break;
	}

	npc.getController().delete_();
}

// Java StonespearReachInstance.java:208-268 (synchronized)
void StonespearReachInstance::checkRank(int32_t rankPoints, bool bossKilled) {
	SYNCHRONIZED(monitor) {
		LegionDominionScore& score = *reward.get();
		cancelAllTasks();
		int32_t rank = 8;
		if (rankPoints > 67000) { // S-Rank
			if (bossKilled) {
				score.setRewardItem1Count(4);
				score.setFinalAP((rankPoints / 10));
			} else {
				score.setRewardItem1Count(3);
			}
			score.setFinalGP(120);
			score.setRewardItem1(186000242); // Ceranium-Medal
			score.setRewardItem2(188053801); // Stonespear Siege Champion Reward Chest
			score.setRewardItem2Count(1);
			score.setRewardItem3(188053804); // Stonespear Siege Champion Relic Chest
			score.setRewardItem3Count(1);
			rank = 1;
		} else if (rankPoints > 41000) { // A-Rank
			score.setFinalGP(100);
			score.setRewardItem1(186000242); // Ceranium-Medal
			score.setRewardItem1Count(2);
			score.setRewardItem2(188053800); // Stonespear Siege Runner-Up Reward Chest
			score.setRewardItem2Count(1);
			score.setRewardItem3(188053803); // Stonespear Siege Runner-Up Relic Chest
			score.setRewardItem3Count(1);
			rank = 2;
		} else if (rankPoints > 25000) { // B-Rank
			score.setFinalGP(80);
			score.setRewardItem1(186000147); // Mithril-Medal
			score.setRewardItem1Count(5);
			score.setRewardItem2(188053799); // Stonespear Siege Reward Chest
			score.setRewardItem2Count(1);
			score.setRewardItem3(188053802); // Stonespear Siege Relic Chest
			score.setRewardItem3Count(1);
			rank = 3;
		} else if (rankPoints > 8800) { // D-Rank
			score.setRewardItem1(186000243); // Ceranium-Fragment
			score.setRewardItem1Count(12);
			score.setRewardItem2(186000096); // Platinum Medal
			score.setRewardItem2Count(3);
			rank = 5;
		} else if (rankPoints > 0) { // F-Rank
			if (rankPoints > 1000) {
				score.setRewardItem1(186000243); // Ceranium-Fragment
				score.setRewardItem1Count(6);
				score.setRewardItem2(186000096); // Platinum Medal
				score.setRewardItem2Count(1);
			}
			rank = 6;
		}

		if (!score.isRewarded()) {
			score.setInstanceProgressionType(InstanceProgressionType::END_PROGRESS);
			score.setRank(rank);
			despawnAll();
			int64_t endTime = commons::utils::currentTimeMillis();
			sendPacket(std::nullopt, 0);
			reward_();
			if (!startTime.get()) // Java: (endTime - startTime) unboxes a null Long
				throw runtime::NullPointerException("StonespearReachInstance.startTime");
			LegionDominionService::getInstance().onFinishInstance(runtime::Ptr<Legion>(instanceLegion.get()), score.getPoints(), (endTime - *startTime.get()));
		}
	}
}

// Java StonespearReachInstance.java:270-283 (Java: reward(), a name the field takes in C++)
void StonespearReachInstance::reward_() {
	runtime::Ref<LegionDominionScore> score = reward.get();
	instance->forEachPlayer([&score](Player& player) {
		ItemService::addItem(player, score->getRewardItem1(), score->getRewardItem1Count(), true);
		ItemService::addItem(player, score->getRewardItem2(), score->getRewardItem2Count(), true);
		ItemService::addItem(player, score->getRewardItem3(), score->getRewardItem3Count(), true);
		if (score->getFinalGP() > 0) {
			GloryPointsService::addGp(player.getObjectId(), model::gameobjects::player::calcResult(Rates::GP, player, score->getFinalGP()));
		}
		if (score->getFinalAp() > 0) {
			AbyssPointsService::addAp(player, score->getFinalAp());
		}
	});
}

// Java StonespearReachInstance.java:285-288
void StonespearReachInstance::startInstance() {
	deleteAliveNpcs({833284});
	startStage1_1();
}

// Java StonespearReachInstance.java:290-302
void StonespearReachInstance::startStage1_1() { // minute 0 - 1
	spawnGuardianStone(instanceRace.get() == Race::ELYOS, 1);
	int32_t npcId = rndNpcId(855765);
	spawnAtPointsTask(50, npcId, -1);
	spawnAtPointsTask(8000, 856305, 0);
	spawnAtPointsTask(8000, npcId, -1);
	spawnAtPointsTask(16000, npcId, -1);
	spawnAtPointsTask(24000, npcId, -1);
	spawnAtPointsTask(32000, npcId, -1);
	spawnAtPointsTask(40000, npcId, -1);
	rndSpawnTask(36000, 856303, 1, 22, 23);
	startStage1_2();
}

// Java StonespearReachInstance.java:304-318
void StonespearReachInstance::startStage1_2() { // minute 1 - 2
	addTask([this] {
		int32_t npcId = rndNpcId(855765);
		rndSpawnTask(50, npcId, 3, 8, 16);
		rndSpawnTask(2000, npcId, 3, 7, 18);
		rndSpawnTask(4000, npcId, 6, 7, 22);
		npcId = rndNpcId(855765);
		rndSpawnTask(6000, npcId, 4, 7, 18);
		rndSpawnTask(7000, npcId, 4, 7, 18);
		rndSpawnTask(8000, npcId, 6, 7, 18);
		rndSpawnTask(11000, npcId, 8, 7, 18);
		rndSpawnTask(25000, 856303, 1, 22, 23);
		startStage1_3();
	}, 60000);
}

// Java StonespearReachInstance.java:320-340
void StonespearReachInstance::startStage1_3() { // minute 2 - 3
	addTask([this] {
		int32_t npcId = rndNpcId(855765);
		// 2k points
		spawn(855764, 230.8977f, 285.5198f, 96.42f, int8_t{80});
		spawn(855764, 211.254f, 264.134f, 96.53f, int8_t{0});
		spawn(855764, 231.2034f, 243.8273f, 96.37f, int8_t{30});
		spawn(855764, 251.3068f, 264.307f, 96.31f, int8_t{60});
		startAethericFieldTask(1);

		rndSpawnTask(500, npcId, 5, 8, 12);
		rndSpawnTask(2500, npcId, 5, 7, 11);
		rndSpawnTask(5000, npcId, 5, 12, 22);
		npcId = rndNpcId(855765);
		rndSpawnTask(10000, npcId, 5, 12, 22);
		rndSpawnTask(13000, npcId, 5, 7, 11);
		rndSpawnTask(16000, npcId, 5, 7, 11);
		rndSpawnTask(26000, 856303, 1, 22, 23);
		startStage1_4();
	}, 60000);
}

// Java StonespearReachInstance.java:342-349
void StonespearReachInstance::startStage1_4() { // minute 3 - 5
	addTask([this] {
		// 12k points + 3k points
		spawn(855776, 231.14f, 264.399f, 96.5f, int8_t{10}); // kaliga 855776
		rndSpawnTask(35000, 856303, 2, 22, 23);
		startStage2_1();
	}, 60000);
}

// Java StonespearReachInstance.java:351-375
void StonespearReachInstance::startStage2_1() { // minute 5 - 6
	addTask([this] {
		spawnGuardianStone(instanceRace.get() == Race::ELYOS, 2);
		int32_t npcId = rndNpcId(855765);
		int32_t npcId2 = rndNpcId(855788);
		// 2k + 800 = 2,8k
		spawnAtPointsTask(50, npcId, 0);
		spawnAtPointsTask(50, npcId, 2);
		spawnAtPointsTask(50, npcId, 4);
		spawnAtPointsTask(50, npcId, 6);
		spawnAtPointsTask(250, npcId2, 1);
		spawnAtPointsTask(250, npcId2, 3);
		spawnAtPointsTask(250, npcId2, 5);
		spawnAtPointsTask(250, npcId2, 7);
		spawnAtPointsTask(6000, npcId, -1);
		spawnAtPointsTask(11000, npcId, -1);
		// 12k points
		if (instance != nullptr && instance->getNpc(856305) == nullptr) {
			spawnAtPointsTask(1000, 856305, 1);
		}
		// 1,5k points
		rndSpawnTask(20000, 856303, 1, 22, 23);
		startStage2_2();
	}, 120000);
}

// Java StonespearReachInstance.java:377-390
void StonespearReachInstance::startStage2_2() { // minute 6 - 7
	addTask([this] {
		int32_t npcId = rndNpcId(855765);
		int32_t npcId2 = rndNpcId(855788);
		// 2,2k points + 1,4k points = 3,6k points
		rndSpawnTask(50, npcId, 6, 11, 21);
		rndSpawnTask(500, npcId, 6, 11, 21);
		rndSpawnTask(1000, npcId, 10, 11, 21);
		rndSpawnTask(1500, npcId2, 7, 11, 21);
		// 1,5k points
		rndSpawnTask(28000, 856303, 1, 22, 23);
		startStage2_3();
	}, 60000);
}

// Java StonespearReachInstance.java:392-413
void StonespearReachInstance::startStage2_3() { // minute 7 - 8
	addTask([this] {
		int32_t npcId = rndNpcId(855765);
		int32_t npcId2 = rndNpcId(855788);
		// 4k points
		spawn(855787, 230.8977f, 285.5198f, 96.42f, int8_t{80});
		spawn(855787, 211.254f, 264.134f, 96.53f, int8_t{0});
		spawn(855787, 231.2034f, 243.8273f, 96.37f, int8_t{30});
		spawn(855787, 251.3068f, 264.307f, 96.31f, int8_t{60});
		startAethericFieldTask(2);

		// 2,6k + 1,2k = 3,8k
		rndSpawnTask(500, npcId, 10, 11, 21);
		rndSpawnTask(4000, npcId, 10, 11, 21);
		rndSpawnTask(7000, npcId, 6, 11, 21);
		rndSpawnTask(10000, npcId2, 6, 11, 21);

		// 3k points
		rndSpawnTask(28000, 856303, 2, 22, 23);
		startStage2_4();
	}, 60000);
}

// Java StonespearReachInstance.java:415-424
void StonespearReachInstance::startStage2_4() { // minute 8 - 10
	addTask([this] {
		// 21k
		spawn(855799, 231.14f, 264.399f, 96.5f, int8_t{10}); // lanmark
		// 3k
		rndSpawnTask(35000, 856303, 1, 22, 23);
		rndSpawnTask(45000, 856303, 1, 22, 23);
		startStage3_1();
	}, 60000);
}

// Java StonespearReachInstance.java:426-448
void StonespearReachInstance::startStage3_1() { // minute 10 - 11
	addTask([this] {
		spawnGuardianStone(instanceRace.get() == Race::ELYOS, 3);
		int32_t npcId = rndNpcId(855765);
		int32_t npcId2 = rndNpcId(855788);
		int32_t npcId3 = rndNpcId(855811);
		// 1,6k + 4,8k + 2,4k 8,8k
		spawnAtPointsTask(50, npcId, -1);
		spawnAtPointsTask(6000, npcId, -1);
		spawnAtPointsTask(13000, npcId2, -1);
		spawnAtPointsTask(20000, npcId2, -1);
		spawnAtPointsTask(27000, npcId2, -1);
		spawnAtPointsTask(33000, npcId3, -1);

		// 12k points
		if (instance != nullptr && instance->getNpc(856305) == nullptr) {
			spawnAtPointsTask(1000, 856305, 2);
		}
		// 1,5k points
		rndSpawnTask(36000, 856303, 1, 22, 23);
		startStage3_2();
	}, 120000);
}

// Java StonespearReachInstance.java:450-467
void StonespearReachInstance::startStage3_2() { // minute 11 - 12
	addTask([this] {
		int32_t npcId = rndNpcId(855765);
		int32_t npcId2 = rndNpcId(855811);
		// 4,2k points + 3,8k = 8k
		rndSpawnTask(100, npcId2, 7, 6, 9);
		spawnAtPointsTask(5000, npcId, -1);
		rndSpawnTask(10000, npcId2, 7, 6, 9);
		spawnAtPointsTask(15000, npcId, -1);
		rndSpawnTask(20000, npcId, 7, 6, 9);
		spawnAtPointsTask(25000, npcId, -1);
		rndSpawnTask(30000, npcId, 7, 6, 9);

		// 1,5k points
		rndSpawnTask(36000, 856303, 1, 22, 23);
		startStage3_3();
	}, 60000);
}

// Java StonespearReachInstance.java:469-486
void StonespearReachInstance::startStage3_3() { // minute 12 - 13
	addTask([this] {
		int32_t npcId = rndNpcId(855765);
		// 6k points
		spawn(855810, 230.8977f, 285.5198f, 96.42f, int8_t{80});
		spawn(855810, 211.254f, 264.134f, 96.53f, int8_t{0});
		spawn(855810, 231.2034f, 243.8273f, 96.37f, int8_t{30});
		spawn(855810, 251.3068f, 264.307f, 96.31f, int8_t{60});
		startAethericFieldTask(3);

		// 2,5k points
		rndSpawnTask(500, npcId, 25, 7, 14);

		// 1,5k points
		rndSpawnTask(25000, 856303, 1, 22, 23);
		startStage3_4();
	}, 60000);
}

// Java StonespearReachInstance.java:488-501
void StonespearReachInstance::startStage3_4() { // minute 13 - 14
	addTask([this] {
		int32_t npcId = rndNpcId(855765);
		// 3k + 2k + 1,5k = 6,5k
		rndSpawnTask(500, 855813, 10, 7, 10);
		rndSpawnTask(2000, npcId, 10, 7, 14);
		rndSpawnTask(6000, npcId, 15, 7, 14);
		rndSpawnTask(10000, npcId, 10, 7, 14);

		// 1,5k points
		rndSpawnTask(15000, 856303, 1, 22, 23);
		startStage3_5();
	}, 60000);
}

// Java StonespearReachInstance.java:503-513
void StonespearReachInstance::startStage3_5() { // minute 14 - 16
	addTask([this] {
		// 30k points
		spawn(855822, 231.14f, 264.399f, 96.5f, int8_t{10}); // stormwing

		// 3k points
		rndSpawnTask(25000, 856303, 1, 22, 23);
		rndSpawnTask(30000, 856303, 1, 22, 23);
		startStage4_1();
	}, 60000);
}

// Java StonespearReachInstance.java:515-532
void StonespearReachInstance::startStage4_1() { // minute 16 - 17
	addTask([this] {
		spawnGuardianStone(instanceRace.get() == Race::ELYOS, 4);
		int32_t npcId = rndNpcId(855765);
		int32_t npcId2 = rndNpcId(855788);
		int32_t npcId3 = rndNpcId(855811);
		// 2k + 1,6k + 4,8k = 8,4k
		rndSpawnTask(500, npcId, 10, 9, 13);
		rndSpawnTask(7000, npcId, 10, 9, 13);
		rndSpawnTask(15000, npcId2, 8, 9, 13);
		rndSpawnTask(22000, 855835, 8, 15, 20);
		rndSpawnTask(30000, npcId3, 8, 9, 13);

		// 1,5k points
		rndSpawnTask(25000, 856303, 1, 22, 23);
		startStage4_2();
	}, 120000);
}

// Java StonespearReachInstance.java:534-556
void StonespearReachInstance::startStage4_2() { // minute 17 - 18
	addTask([this] {
		// 1k + 2k + 6k = 9k
		rndSpawnTask(500, 855765, 10, 9, 13);
		rndSpawnTask(2000, 855788, 10, 9, 13);

		spawnAtPointsTask(13000, 855835, 0);
		spawnAtPointsTask(14500, 855836, 1);
		spawnAtPointsTask(16000, 855835, 2);
		spawnAtPointsTask(17500, 855836, 3);
		spawnAtPointsTask(19000, 855835, 4);
		spawnAtPointsTask(20500, 855836, 5);
		spawnAtPointsTask(22000, 855835, 6);
		spawnAtPointsTask(23500, 855836, 7);

		rndSpawnTask(27000, 855835, 7, 12, 20);

		// 1,5k points
		rndSpawnTask(15000, 856303, 1, 22, 23);

		startStage4_3();
	}, 60000);
}

// Java StonespearReachInstance.java:558-596
void StonespearReachInstance::startStage4_3() { // minute 18 - 19
	addTask([this] {
		// 1,5k + 1k + 9,6k = 12,1k
		rndSpawnTask(500, 855765, 15, 9, 13);
		rndSpawnTask(2000, 855788, 5, 9, 13);

		spawnAtPointsTask(1000, 855836, 0);
		spawnAtPointsTask(1000, 855835, 1);
		spawnAtPointsTask(3000, 855836, 2);
		spawnAtPointsTask(3000, 855835, 3);
		spawnAtPointsTask(5000, 855836, 4);
		spawnAtPointsTask(5000, 855835, 5);
		spawnAtPointsTask(7000, 855836, 6);
		spawnAtPointsTask(7000, 855835, 7);

		spawnAtPointsTask(9000, 855836, 0);
		spawnAtPointsTask(90000, 855835, 1); // Java: 90000 (the neighbours' pattern says 9000; ported as written)
		spawnAtPointsTask(11000, 855836, 2);
		spawnAtPointsTask(11000, 855835, 3);
		spawnAtPointsTask(13000, 855836, 4);
		spawnAtPointsTask(13000, 855835, 5);
		spawnAtPointsTask(15000, 855836, 6);
		spawnAtPointsTask(15000, 855835, 7);

		spawnAtPointsTask(18000, 855836, 0);
		spawnAtPointsTask(18000, 855835, 1);
		spawnAtPointsTask(20000, 855836, 2);
		spawnAtPointsTask(20000, 855835, 3);
		spawnAtPointsTask(22000, 855836, 4);
		spawnAtPointsTask(22000, 855835, 5);
		spawnAtPointsTask(24000, 855836, 6);
		spawnAtPointsTask(24000, 855835, 7);

		// 3k points
		rndSpawnTask(10000, 856303, 1, 22, 23);
		rndSpawnTask(20000, 856303, 1, 22, 23);
		startStage4_4();
	}, 60000);
}

// Java StonespearReachInstance.java:598-629
void StonespearReachInstance::startStage4_4() { // minute 19 - 20
	addTask([this] {
		// 12k points + 8k points = 20k points
		// 2k + 6,4k + 6,4k = 14,8k
		int32_t npcId = rndNpcId(855788);
		rndSpawnTask(500, 855765, 20, 9, 20);

		rndSpawnTask(8000, 855837, 4, 7, 9);
		rndSpawnTask(9000, npcId, 4, 7, 20);
		rndSpawnTask(10000, npcId, 4, 7, 20);

		npcId = rndNpcId(855765);
		rndSpawnTask(16000, 855837, 4, 7, 9);
		rndSpawnTask(17000, npcId, 4, 7, 20);
		rndSpawnTask(18000, npcId, 4, 7, 20);

		npcId = rndNpcId(855765);
		rndSpawnTask(24000, 855837, 4, 7, 9);
		rndSpawnTask(25000, npcId, 4, 7, 20);
		rndSpawnTask(26000, npcId, 4, 7, 20);

		npcId = rndNpcId(855765);
		rndSpawnTask(32000, 855837, 4, 7, 9);
		rndSpawnTask(33000, npcId, 4, 7, 20);
		rndSpawnTask(34000, npcId, 4, 7, 20);

		// 3k points
		rndSpawnTask(20000, 856303, 1, 22, 23);
		rndSpawnTask(25000, 856303, 1, 22, 23);
		startStage4_5();
	}, 60000);
}

// Java StonespearReachInstance.java:631-654
void StonespearReachInstance::startStage4_5() { // minute 20 - 21
	addTask([this] {
		int32_t npcId = rndNpcId(855765);
		int32_t npcId2 = rndNpcId(855788);
		// 8k
		spawn(855833, 230.8977f, 285.5198f, 96.42f, int8_t{80});
		spawn(855833, 211.254f, 264.134f, 96.53f, int8_t{0});
		spawn(855833, 231.2034f, 243.8273f, 96.37f, int8_t{30});
		spawn(855833, 251.3068f, 264.307f, 96.31f, int8_t{60});
		startAethericFieldTask(4);

		// 2k + 2k + 3,2k = 7,2k
		rndSpawnTask(500, npcId, 20, 9, 20);
		rndSpawnTask(5000, npcId2, 10, 7, 20);
		rndSpawnTask(9000, 855835, 4, 7, 20);
		rndSpawnTask(11000, 855836, 4, 7, 20);

		// 3k
		rndSpawnTask(20000, 856303, 1, 22, 23);
		rndSpawnTask(25000, 856303, 1, 22, 23);

		startStage4_6();
	}, 60000);
}

// Java StonespearReachInstance.java:656-663
void StonespearReachInstance::startStage4_6() { // minute 21 - 30
	// 42k
	spawn(855843, 231.14f, 264.399f, 96.5f, int8_t{10}); // general of illusion
	// 6k
	rndSpawnTask(25000, 856303, 1, 22, 23);
	rndSpawnTask(30000, 856303, 1, 22, 23);
	rndSpawnTask(35000, 856303, 1, 22, 23);
}

// Java StonespearReachInstance.java:665-667
void StonespearReachInstance::startFailTask() {
	failTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { checkRank(reward.get()->getPoints(), false); }, 1800000)); // 30min
}

// Java StonespearReachInstance.java:669-675
void StonespearReachInstance::addPoints(Npc& npc, int32_t value) { // Java: int points (the field's name)
	if (reward.get()->isStartProgress()) {
		reward.get()->addPoints(value);
		sendPacket(npc.getObjectTemplate()->getL10n(), value);
		// TODO points >= 100mio -> stop. But its actually impossible to reach 100mio points.. I'll just leave it out
	}
}

// Java StonespearReachInstance.java:677-681
void StonespearReachInstance::sendPacket(std::optional<std::string_view> npcL10n, int32_t value) { // Java: int points
	if (npcL10n)
		PacketSendUtility::broadcastToMap(*instance, SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE(*npcL10n, value));
	runtime::Ref<LegionDominionScoreWriter> writer = LegionDominionScoreWriter::create(*reward.get());
	PacketSendUtility::broadcastToMap(*instance, SM_INSTANCE_SCORE(instance->getMapId(), *writer, getTime()));
}

// Java StonespearReachInstance.java:683-691
int32_t StonespearReachInstance::getTime() {
	if (!startTime.get()) // Java: System.currentTimeMillis() - startTime unboxes a null Long
		throw runtime::NullPointerException("StonespearReachInstance.startTime");
	int64_t result = commons::utils::currentTimeMillis() - *startTime.get();
	if (reward.get()->isPreparing()) {
		return static_cast<int32_t>(180000 - result);
	} else if (result < 1800000) {
		return static_cast<int32_t>(1800000 - result);
	}
	return 0;
}

// Java StonespearReachInstance.java:693-702
void StonespearReachInstance::spawnAtPointsTask(int32_t delay, int32_t npcId, int32_t index) {
	if (!reward.get()->isStartProgress()) {
		return;
	}
	addTask([this, npcId, index] {
		if (reward.get()->isStartProgress()) {
			spawn(npcId, index);
		}
	}, delay);
}

// Java StonespearReachInstance.java:704-721 (Java's points.get(index) throws past the eight points)
void StonespearReachInstance::spawn(int32_t npcId, int32_t index) {
	if (!reward.get()->isStartProgress()) {
		return;
	}
	if (index >= 0) {
		runtime::Ptr<WorldPosition> point = points.get(index);
		if (point != nullptr) {
			spawn(npcId, point->getX(), point->getY(), point->getZ(), point->getHeading());
		}
	} else {
		for (const runtime::Ref<WorldPosition>& point : points.snapshot()) {
			if (!reward.get()->isStartProgress()) {
				break;
			}
			spawn(npcId, point->getX(), point->getY(), point->getZ(), point->getHeading());
		}
	}
}

// Java StonespearReachInstance.java:723-732
void StonespearReachInstance::rndSpawnTask(int32_t delay, int32_t npcId, int32_t amount, int32_t minRange, int32_t maxRange) {
	if (!reward.get()->isStartProgress()) {
		return;
	}
	addTask([this, npcId, amount, minRange, maxRange] {
		if (reward.get()->isStartProgress()) {
			rndSpawnInRange(npcId, amount, minRange, maxRange);
		}
	}, delay);
}

// Java StonespearReachInstance.java:734-741
void StonespearReachInstance::rndSpawnInRange(int32_t npcId, int32_t amount, int32_t minRange, int32_t maxRange) {
	for (int32_t i = 0; i < amount; i++) {
		if (!reward.get()->isStartProgress()) {
			break;
		}
		spawnAtRandomPositionWithinRange(npcId, minRange, maxRange);
	}
}

// Java StonespearReachInstance.java:743-758
void StonespearReachInstance::spawnAtRandomPositionWithinRange(int32_t npcId, int32_t minRange, int32_t maxRange) {
	if (!reward.get()->isStartProgress()) {
		return;
	}
	std::vector<int32_t> ranges; // Java: IntStream.range(minRange, maxRange + 1).boxed().collect(Collectors.toList())
	for (int32_t range = minRange; range < maxRange + 1; range++)
		ranges.push_back(range);
	std::shuffle(ranges.begin(), ranges.end(), commons::utils::Rnd::generator()); // Java: Collections.shuffle(ranges)
	double angleRadians = commons::utils::Rnd::nextFloat(360.0f) * std::numbers::pi / 180.0; // Java: Math.toRadians
	for (int32_t range : ranges) {
		float x = 231.14f + static_cast<float>(std::cos(angleRadians) * range);
		float y = 264.399f + static_cast<float>(std::sin(angleRadians) * range);
		if (isValidPoint(x, y)) {
			spawn(npcId, x, y, 96.51f, int8_t{50});
			break;
		}
	}
}

// Java StonespearReachInstance.java:760-776
void StonespearReachInstance::spawnGuardianStone(bool isElyos, int32_t stage) {
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_OBJ_Start());
	switch (stage) {
		case 1:
			spawn((isElyos ? 855763 : 856466), 231.14f, 264.399f, 96.5f, int8_t{10});
			break;
		case 2:
			spawn((isElyos ? 855786 : 856467), 231.14f, 264.399f, 96.5f, int8_t{10});
			break;
		case 3:
			spawn((isElyos ? 855809 : 856468), 231.14f, 264.399f, 96.5f, int8_t{10});
			break;
		case 4:
			spawn((isElyos ? 855832 : 856469), 231.14f, 264.399f, 96.5f, int8_t{10});
			break;
		default:
			break;
	}
}

// Java StonespearReachInstance.java:778-781
bool StonespearReachInstance::isValidPoint(float x, float y) {
	return (PositionUtil::getDistance(x, y, 211.254f, 264.134f) >= 2.5) && (PositionUtil::getDistance(x, y, 230.8977f, 285.5198f) >= 2.5)
		&& (PositionUtil::getDistance(x, y, 251.3068f, 264.307f) >= 2.5) && (PositionUtil::getDistance(x, y, 231.2034f, 243.8273f) >= 2.5);
}

// Java StonespearReachInstance.java:783-792
void StonespearReachInstance::addWorldPoints() {
	points.add(WorldPosition::create(mapId, 208.4323f, 264.0647f, 96.223f, int8_t{1}));
	points.add(WorldPosition::create(mapId, 214.5657f, 281.1983f, 96.1398f, int8_t{99}));
	points.add(WorldPosition::create(mapId, 230.9258f, 288.3556f, 96.5095f, int8_t{87}));
	points.add(WorldPosition::create(mapId, 248.5251f, 281.1702f, 96.3423f, int8_t{74}));
	points.add(WorldPosition::create(mapId, 254.0089f, 264.1608f, 96.1255f, int8_t{60}));
	points.add(WorldPosition::create(mapId, 248.0067f, 247.8215f, 96.0116f, int8_t{47}));
	points.add(WorldPosition::create(mapId, 231.1899f, 240.6733f, 96.1348f, int8_t{30}));
	points.add(WorldPosition::create(mapId, 214.2921f, 247.5521f, 96.267f, int8_t{20}));
}

// Java StonespearReachInstance.java:794-796
void StonespearReachInstance::despawnAll() {
	instance->forEachNpc([](Npc& npc) { npc.getController().delete_(); });
}

// Java StonespearReachInstance.java:798-803
void StonespearReachInstance::cancelAllTasks() {
	runtime::FutureRef fail = failTask.get();
	if (fail != nullptr && !fail->isCancelled()) {
		fail->cancel(false);
	}
	for (const runtime::FutureRef& future : tasks.snapshot()) {
		if (future != nullptr && !future->isCancelled())
			future->cancel(true);
	}
}

// Java StonespearReachInstance.java:805-809
bool StonespearReachInstance::onDie(Player& player, Creature& lastAttacker) {
	static_cast<void>(player);
	static_cast<void>(lastAttacker);
	checkInstance();
	return true;
}

// Java StonespearReachInstance.java:811-822
bool StonespearReachInstance::onReviveEvent(Player& player) {
	if (!deadPlayers.contains(player.getObjectId())) {
		deadPlayers.add(player.getObjectId());
	}
	PlayerReviveService::revive(player, 100, 100, false, 0);
	player.getGameStats()->updateStatsAndSpeedVisually();
	TeleportService::teleportTo(player, *instance, 152, 264, 103);
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_LEGION_DOMINION_MOVE_BIRTHAREA_FRIENDLY(player.getName()));
	checkInstance();
	return true;
}

// Java StonespearReachInstance.java:824-834
void StonespearReachInstance::onInstanceDestroy() {
	cancelAllTasks();
	reward.get()->clear();
	deadPlayers.clear();
	allowedPlayers.clear();
	instanceLegion = runtime::Ref<Legion>();
	instanceRace = std::nullopt;
	points.clear();
	despawnAll();
}

// Java StonespearReachInstance.java:836-843
bool StonespearReachInstance::allPlayersAreDead() {
	for (const runtime::Ptr<Player>& p : instance->getPlayersInside()) {
		if (!p->isDead()) {
			return false;
		}
	}
	return true;
}

// Java StonespearReachInstance.java:845-849
void StonespearReachInstance::checkInstance() {
	if (deadPlayers.size() >= allowedPlayers.size() || allPlayersAreDead()) {
		checkRank(reward.get()->getPoints(), false);
	}
}

// Java StonespearReachInstance.java:851-881 (synchronized)
bool StonespearReachInstance::canEnter(Player& p) {
	SYNCHRONIZED(monitor) {
		if (reward.get()->getInstanceProgressionType() == InstanceProgressionType::END_PROGRESS) {
			PacketSendUtility::sendPacket(p, SM_SYSTEM_MESSAGE(1402947, std::vector<std::string>{}));
			return false;
		}
		runtime::Ptr<Legion> legion = instanceLegion.get();
		if (legion != nullptr) {
			if (p.getLegion() != nullptr) {
				if (p.getLegion()->getCurrentLegionDominion() > 0) {
					if (reward.get()->getInstanceProgressionType() == InstanceProgressionType::PREPARING) {
						if (p.getLegion()->getLegionId() == legion->getLegionId()) {
							if (allowedPlayers.size() < 12 || allowedPlayers.contains(p.getObjectId())) {
								return true;
							} else {
								PacketSendUtility::sendPacket(p, SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_TOO_MANY_MEMBERS(12, mapId));
							}
						} else {
							PacketSendUtility::sendPacket(p, SM_SYSTEM_MESSAGE(1402907, std::vector<std::string>{}));
						}
					} else {
						PacketSendUtility::sendPacket(p, SM_SYSTEM_MESSAGE(1400235, std::vector<std::string>{}));
					}
				} else {
					PacketSendUtility::sendPacket(p, SM_SYSTEM_MESSAGE(1402937, std::vector<std::string>{}));
				}
			} else {
				PacketSendUtility::sendPacket(p, SM_SYSTEM_MESSAGE(1402906, std::vector<std::string>{}));
			}
		}
		return false;
	}
	return false; // not reached: the block above returns
}

// Java StonespearReachInstance.java:883-912
void StonespearReachInstance::leaveInstance(Player& player) {
	runtime::Ptr<Legion> legion = instanceLegion.get();
	if (legion != nullptr) {
		switch (legion->getCurrentLegionDominion()) {
			case 1:
				TeleportService::teleportTo(player, 220080000, 1770, 2556, 300);
				break;
			case 2:
				TeleportService::teleportTo(player, 220080000, 1474, 1748, 330);
				break;
			case 3:
				TeleportService::teleportTo(player, 220080000, 765, 1278, 255);
				break;
			case 4:
				TeleportService::teleportTo(player, 210070000, 1356, 622, 584);
				break;
			case 5:
				TeleportService::teleportTo(player, 210070000, 1195, 1607, 469);
				break;
			case 6:
				TeleportService::teleportTo(player, 210070000, 2361, 1515, 440);
				break;
			default:
				TeleportService::moveToBindLocation(player);
				break;
		}
	} else {
		TeleportService::moveToBindLocation(player);
	}
}

// Java StonespearReachInstance.java:914-924
void StonespearReachInstance::startAethericFieldTask(int32_t stage) {
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_OBJ_Bomb());
	addTask([this, stage] {
		if (reward.get()->isStartProgress()) {
			if (aethericKills.get() < stage * 4) {
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_OBJ_Bomb_Die());
				checkRank(reward.get()->getPoints(), false);
			}
		}
	}, 120000);
}

} // namespace aion::gameserver::handlers::instance
