#include "aion/gameserver/handlers/instance/NightmareCircus.h"

#include <string>

#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h" // the Ref<Effect> applyEffectDirectly returns
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(NightmareCircus, 301200000);

// Java NightmareCircus.java:34-36
NightmareCircus::NightmareCircus(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<NightmareCircus> NightmareCircus::create(WorldMapInstance& instance) {
	return runtime::makeRef<NightmareCircus>(instance);
}

// Java NightmareCircus.java:38-46 (the lambda keeps the player alive, as Java's capture does)
void NightmareCircus::onEnterInstance(Player& player) {
	runtime::Ref<Player> entered(player);
	ThreadPoolManager::getInstance().schedule({this}, [this, entered] {
		SkillEngine::getInstance().applyEffectDirectly(entered->getRace() == Race::ELYOS ? 21469 : 21471, *entered, *entered);
		sendMsg(831551, 1501120);
		sendMsg(831552, 1501119);
		sendMsg(831553, 1501118);
	}, 1000);
}

// Java NightmareCircus.java:48-54
void NightmareCircus::cancelDespawnBossTask() {
	for (int32_t i = 0; i < despawnTasks->length(); i++) {
		runtime::FutureRef despawnTask = despawnTasks->get(i);
		if (despawnTask != nullptr && !despawnTask->isCancelled()) {
			despawnTask->cancel(true);
		}
	}
}

// Java NightmareCircus.java:56-66
void NightmareCircus::onPlayMovieEnd(Player& player, int32_t movieId) {
	if (movieId == 983) {
		if (moviePlayed.compareAndSet(false, true)) {
			sendMsg(831747, 1501114);
			ThreadPoolManager::getInstance().schedule({this}, [this] { deleteAliveNpcs({831747}); }, 3000);
			spawnPhase();
			startDespawnBossTask();
		}
	}
}

// Java NightmareCircus.java:68-257
void NightmareCircus::spawnPhase() {
	// Left
	// Wave 1
	sp(233455, 524.41406f, 628.0354f, 207.97612f, int8_t{90}, 0, "3012000001");
	sp(233455, 521.2755f, 628.16846f, 208.06583f, int8_t{90}, 0, "3012000002");
	sp(233455, 524.4364f, 626.2443f, 207.68132f, int8_t{90}, 0, "3012000003");
	sp(233455, 521.329f, 626.74036f, 207.89403f, int8_t{90}, 0, "3012000004");

	// Wave 2
	sp(233450, 524.41406f, 628.0354f, 207.97612f, int8_t{90}, 10000, "3012000001");
	sp(233450, 521.2755f, 628.16846f, 208.06583f, int8_t{90}, 10000, "3012000002");
	sp(233450, 524.4364f, 626.2443f, 207.68132f, int8_t{90}, 10000, "3012000003");
	sp(233450, 521.329f, 626.74036f, 207.89403f, int8_t{90}, 10000, "3012000004");

	// Wave 3
	sp(233455, 524.41406f, 628.0354f, 207.97612f, int8_t{90}, 20000, "3012000001");
	sp(233455, 521.2755f, 628.16846f, 208.06583f, int8_t{90}, 20000, "3012000002");
	sp(233455, 524.4364f, 626.2443f, 207.68132f, int8_t{90}, 20000, "3012000003");
	sp(233455, 521.329f, 626.74036f, 207.89403f, int8_t{90}, 20000, "3012000004");

	// Wave 4
	sp(233450, 524.41406f, 628.0354f, 207.97612f, int8_t{90}, 30000, "3012000001");
	sp(233450, 521.2755f, 628.16846f, 208.06583f, int8_t{90}, 30000, "3012000002");
	sp(233450, 524.4364f, 626.2443f, 207.68132f, int8_t{90}, 30000, "3012000003");
	sp(233450, 521.329f, 626.74036f, 207.89403f, int8_t{90}, 30000, "3012000004");

	// Wave 5
	sp(233455, 524.41406f, 628.0354f, 207.97612f, int8_t{90}, 40000, "3012000001");
	sp(233455, 521.2755f, 628.16846f, 208.06583f, int8_t{90}, 40000, "3012000002");
	sp(233455, 524.4364f, 626.2443f, 207.68132f, int8_t{90}, 40000, "3012000003");
	sp(233455, 521.329f, 626.74036f, 207.89403f, int8_t{90}, 40000, "3012000004");

	// Wave 6
	sp(233450, 524.41406f, 628.0354f, 207.97612f, int8_t{90}, 50000, "3012000001");
	sp(233450, 521.2755f, 628.16846f, 208.06583f, int8_t{90}, 50000, "3012000002");
	sp(233450, 524.4364f, 626.2443f, 207.68132f, int8_t{90}, 50000, "3012000003");
	sp(233450, 521.329f, 626.74036f, 207.89403f, int8_t{90}, 50000, "3012000004");

	// Wave 7
	sp(233455, 524.41406f, 628.0354f, 207.97612f, int8_t{90}, 60000, "3012000001");
	sp(233455, 521.2755f, 628.16846f, 208.06583f, int8_t{90}, 60000, "3012000002");
	sp(233455, 524.4364f, 626.2443f, 207.68132f, int8_t{90}, 60000, "3012000003");
	sp(233455, 521.329f, 626.74036f, 207.89403f, int8_t{90}, 60000, "3012000004");

	// Wave 8
	sp(233450, 524.41406f, 628.0354f, 207.97612f, int8_t{90}, std::optional<int32_t>(1501151), 70000, "3012000001");
	sp(233450, 521.2755f, 628.16846f, 208.06583f, int8_t{90}, 70000, "3012000002");
	sp(233450, 524.4364f, 626.2443f, 207.68132f, int8_t{90}, 70000, "3012000003");
	sp(233450, 521.329f, 626.74036f, 207.89403f, int8_t{90}, std::optional<int32_t>(1501151), 70000, "3012000004");

	// Wave 9
	sp(233455, 524.41406f, 628.0354f, 207.97612f, int8_t{90}, 80000, "3012000001");
	sp(233455, 521.2755f, 628.16846f, 208.06583f, int8_t{90}, 80000, "3012000002");
	sp(233455, 524.4364f, 626.2443f, 207.68132f, int8_t{90}, 80000, "3012000003");
	sp(233455, 521.329f, 626.74036f, 207.89403f, int8_t{90}, 80000, "3012000004");

	// Wave 10
	sp(233450, 524.41406f, 628.0354f, 207.97612f, int8_t{90}, 90000, "3012000001");
	sp(233450, 521.2755f, 628.16846f, 208.06583f, int8_t{90}, 90000, "3012000002");
	sp(233450, 524.4364f, 626.2443f, 207.68132f, int8_t{90}, 90000, "3012000003");
	sp(233450, 521.329f, 626.74036f, 207.89403f, int8_t{90}, 90000, "3012000004");

	// Wave 11
	sp(233455, 524.41406f, 628.0354f, 207.97612f, int8_t{90}, 100000, "3012000001");
	sp(233455, 521.2755f, 628.16846f, 208.06583f, int8_t{90}, 100000, "3012000002");
	sp(233455, 524.4364f, 626.2443f, 207.68132f, int8_t{90}, 100000, "3012000003");
	sp(233455, 521.329f, 626.74036f, 207.89403f, int8_t{90}, 100000, "3012000004");

	// Wave 12
	sp(233450, 524.41406f, 628.0354f, 207.97612f, int8_t{90}, 110000, "3012000001");
	sp(233450, 521.2755f, 628.16846f, 208.06583f, int8_t{90}, 110000, "3012000002");
	sp(233450, 524.4364f, 626.2443f, 207.68132f, int8_t{90}, 110000, "3012000003");
	sp(233450, 521.329f, 626.74036f, 207.89403f, int8_t{90}, 110000, "3012000004");

	// Wave 13
	sp(233455, 524.41406f, 628.0354f, 207.97612f, int8_t{90}, 120000, "3012000001");
	sp(233455, 521.2755f, 628.16846f, 208.06583f, int8_t{90}, 120000, "3012000002");
	sp(233455, 524.4364f, 626.2443f, 207.68132f, int8_t{90}, 120000, "3012000003");
	sp(233455, 521.329f, 626.74036f, 207.89403f, int8_t{90}, 120000, "3012000004");

	// Wave 14
	sp(233450, 524.41406f, 628.0354f, 207.97612f, int8_t{90}, 130000, "3012000001");
	sp(233450, 521.2755f, 628.16846f, 208.06583f, int8_t{90}, 130000, "3012000002");
	sp(233450, 524.4364f, 626.2443f, 207.68132f, int8_t{90}, 130000, "3012000003");
	sp(233450, 521.329f, 626.74036f, 207.89403f, int8_t{90}, 130000, "3012000004");

	// Wave 15
	sp(233455, 524.41406f, 628.0354f, 207.97612f, int8_t{90}, std::optional<int32_t>(1501132), 140000, "3012000001");
	sp(233455, 521.2755f, 628.16846f, 208.06583f, int8_t{90}, std::optional<int32_t>(1501152), 140000, "3012000002");
	sp(233455, 524.4364f, 626.2443f, 207.68132f, int8_t{90}, std::optional<int32_t>(1501132), 140000, "3012000003");
	sp(233455, 521.329f, 626.74036f, 207.89403f, int8_t{90}, std::optional<int32_t>(1501152), 140000, "3012000004");

	// Right
	// Wave 1
	sp(233450, 523.77045f, 494.52615f, 198.37659f, int8_t{90}, 0, "3012000005");
	sp(233450, 521.3197f, 494.83768f, 198.40182f, int8_t{90}, 0, "3012000006");
	sp(233450, 523.791f, 496.6259f, 198.52597f, int8_t{90}, 0, "3012000007");
	sp(233450, 521.4548f, 496.7582f, 198.46896f, int8_t{90}, 0, "3012000008");

	// Wave 2
	sp(233455, 523.77045f, 494.52615f, 198.37659f, int8_t{90}, 10000, "3012000005");
	sp(233455, 521.3197f, 494.83768f, 198.40182f, int8_t{90}, 10000, "3012000006");
	sp(233455, 523.791f, 496.6259f, 198.52597f, int8_t{90}, 10000, "3012000007");
	sp(233455, 521.4548f, 496.7582f, 198.46896f, int8_t{90}, 10000, "3012000008");

	// Wave 3
	sp(233450, 523.77045f, 494.52615f, 198.37659f, int8_t{90}, 20000, "3012000005");
	sp(233450, 521.3197f, 494.83768f, 198.40182f, int8_t{90}, 20000, "3012000006");
	sp(233450, 523.791f, 496.6259f, 198.52597f, int8_t{90}, 20000, "3012000007");
	sp(233450, 521.4548f, 496.7582f, 198.46896f, int8_t{90}, 20000, "3012000008");

	// Wave 4
	sp(233455, 523.77045f, 494.52615f, 198.37659f, int8_t{90}, 30000, "3012000005");
	sp(233455, 521.3197f, 494.83768f, 198.40182f, int8_t{90}, 30000, "3012000006");
	sp(233455, 523.791f, 496.6259f, 198.52597f, int8_t{90}, 30000, "3012000007");
	sp(233455, 521.4548f, 496.7582f, 198.46896f, int8_t{90}, 30000, "3012000008");

	// Wave 5
	sp(233450, 523.77045f, 494.52615f, 198.37659f, int8_t{90}, 40000, "3012000005");
	sp(233450, 521.3197f, 494.83768f, 198.40182f, int8_t{90}, 40000, "3012000006");
	sp(233450, 523.791f, 496.6259f, 198.52597f, int8_t{90}, 40000, "3012000007");
	sp(233450, 521.4548f, 496.7582f, 198.46896f, int8_t{90}, 40000, "3012000008");

	// Wave 6
	sp(233455, 523.77045f, 494.52615f, 198.37659f, int8_t{90}, 50000, "3012000005");
	sp(233455, 521.3197f, 494.83768f, 198.40182f, int8_t{90}, 50000, "3012000006");
	sp(233455, 523.791f, 496.6259f, 198.52597f, int8_t{90}, 50000, "3012000007");
	sp(233455, 521.4548f, 496.7582f, 198.46896f, int8_t{90}, 50000, "3012000008");

	// Wave 7
	sp(233450, 523.77045f, 494.52615f, 198.37659f, int8_t{90}, 60000, "3012000005");
	sp(233450, 521.3197f, 494.83768f, 198.40182f, int8_t{90}, 60000, "3012000006");
	sp(233450, 523.791f, 496.6259f, 198.52597f, int8_t{90}, 60000, "3012000007");
	sp(233450, 521.4548f, 496.7582f, 198.46896f, int8_t{90}, 60000, "3012000008");

	// Wave 8
	sp(233455, 523.77045f, 494.52615f, 198.37659f, int8_t{90}, 70000, "3012000005");
	sp(233455, 521.3197f, 494.83768f, 198.40182f, int8_t{90}, 70000, "3012000006");
	sp(233455, 523.791f, 496.6259f, 198.52597f, int8_t{90}, 70000, "3012000007");
	sp(233455, 521.4548f, 496.7582f, 198.46896f, int8_t{90}, 70000, "3012000008");

	// Wave 9
	sp(233450, 523.77045f, 494.52615f, 198.37659f, int8_t{90}, std::optional<int32_t>(1501151), 80000, "3012000005");
	sp(233450, 521.3197f, 494.83768f, 198.40182f, int8_t{90}, 80000, "3012000006");
	sp(233450, 523.791f, 496.6259f, 198.52597f, int8_t{90}, 80000, "3012000007");
	sp(233450, 521.4548f, 496.7582f, 198.46896f, int8_t{90}, std::optional<int32_t>(1501151), 80000, "3012000008");

	// Wave 10
	sp(233455, 523.77045f, 494.52615f, 198.37659f, int8_t{90}, 90000, "3012000005");
	sp(233455, 521.3197f, 494.83768f, 198.40182f, int8_t{90}, 90000, "3012000006");
	sp(233455, 523.791f, 496.6259f, 198.52597f, int8_t{90}, 90000, "3012000007");
	sp(233455, 521.4548f, 496.7582f, 198.46896f, int8_t{90}, 90000, "3012000008");

	// Wave 11
	sp(233450, 523.77045f, 494.52615f, 198.37659f, int8_t{90}, 100000, "3012000005");
	sp(233450, 521.3197f, 494.83768f, 198.40182f, int8_t{90}, 100000, "3012000006");
	sp(233450, 523.791f, 496.6259f, 198.52597f, int8_t{90}, 100000, "3012000007");
	sp(233450, 521.4548f, 496.7582f, 198.46896f, int8_t{90}, 100000, "3012000008");

	// Wave 12
	sp(233455, 523.77045f, 494.52615f, 198.37659f, int8_t{90}, 110000, "3012000005");
	sp(233455, 521.3197f, 494.83768f, 198.40182f, int8_t{90}, 110000, "3012000006");
	sp(233455, 523.791f, 496.6259f, 198.52597f, int8_t{90}, 110000, "3012000007");
	sp(233455, 521.4548f, 496.7582f, 198.46896f, int8_t{90}, 110000, "3012000008");

	// Wave 13
	sp(233450, 523.77045f, 494.52615f, 198.37659f, int8_t{90}, 120000, "3012000005");
	sp(233450, 521.3197f, 494.83768f, 198.40182f, int8_t{90}, 120000, "3012000006");
	sp(233450, 523.791f, 496.6259f, 198.52597f, int8_t{90}, 120000, "3012000007");
	sp(233450, 521.4548f, 496.7582f, 198.46896f, int8_t{90}, 120000, "3012000008");

	// Wave 14
	sp(233455, 523.77045f, 494.52615f, 198.37659f, int8_t{90}, 130000, "3012000005");
	sp(233455, 521.3197f, 494.83768f, 198.40182f, int8_t{90}, 130000, "3012000006");
	sp(233455, 523.791f, 496.6259f, 198.52597f, int8_t{90}, 130000, "3012000007");
	sp(233455, 521.4548f, 496.7582f, 198.46896f, int8_t{90}, 130000, "3012000008");

	// Wave 15
	sp(233450, 523.77045f, 494.52615f, 198.37659f, int8_t{90}, std::optional<int32_t>(1501132), 140000, "3012000005");
	sp(233450, 521.3197f, 494.83768f, 198.40182f, int8_t{90}, std::optional<int32_t>(1501152), 140000, "3012000006");
	sp(233450, 523.791f, 496.6259f, 198.52597f, int8_t{90}, std::optional<int32_t>(1501132), 140000, "3012000007");
	sp(233450, 521.4548f, 496.7582f, 198.46896f, int8_t{90}, std::optional<int32_t>(1501152), 140000, "3012000008");

	// Mistress Viloa
	sp(233459, 549.0792f, 565.6647f, 198.83736f, int8_t{60}, 1501136, 160000);
	// Harlequin Lord Reshka
	sp(233453, 549.0792f, 565.6647f, 198.83736f, int8_t{60}, 1501143, 200000);
	// Nightmare Lord Heiramune
	sp(233467, 549.0792f, 565.6647f, 198.83736f, int8_t{60}, 1501131, 300000);
}

// Java NightmareCircus.java:259-266
void NightmareCircus::sp(int32_t npcId, float x, float y, float z, int8_t h, int32_t msg, int32_t time) {
	ThreadPoolManager::getInstance().schedule({this}, [this, npcId, x, y, z, h, msg] {
		if (!isInstanceDestroyed.get()) {
			spawn(npcId, x, y, z, h);
			sendMsg(npcId, msg);
		}
	}, time);
}

// Java NightmareCircus.java:268-270
void NightmareCircus::sp(int32_t npcId, float x, float y, float z, int8_t h, int32_t time, std::string_view walkerId) {
	sp(npcId, x, y, z, h, std::nullopt, time, walkerId);
}

// Java NightmareCircus.java:272-282
void NightmareCircus::sp(int32_t npcId, float x, float y, float z, int8_t h, std::optional<int32_t> msg, int32_t time, std::string_view walkerId) {
	ThreadPoolManager::getInstance().schedule({this}, [this, npcId, x, y, z, h, msg, walker = std::string(walkerId)] {
		if (!isInstanceDestroyed.get()) {
			runtime::Ptr<Npc> npc = runtime::cast<Npc>(spawn(npcId, x, y, z, h));
			npc->getSpawn()->setWalkerId(walker);
			WalkManager::startWalking(dynamic_cast<gameserver::ai::NpcAI&>(npc->getAi())); // Java: (NpcAI) npc.getAi()
			if (msg)
				sendMsg(npcId, *msg);
		}
	}, time);
}

// Java NightmareCircus.java:284-287
void NightmareCircus::startDespawnBossTask() {
	(*despawnTasks)[0] = ThreadPoolManager::getInstance().schedule({this}, [this] { deleteAliveNpcs({233459}); }, 210000);
	(*despawnTasks)[1] = ThreadPoolManager::getInstance().schedule({this}, [this] { deleteAliveNpcs({233453}); }, 310000);
}

// Java NightmareCircus.java:289-341
void NightmareCircus::onDie(Npc& npc) {
	switch (npc.getNpcId()) {
		case 233450:
		case 233455:
		case 233462:
		case 233463:
			npc.getController().delete_();
			break;
		case 233467:
			PacketSendUtility::broadcastToMap(*instance, SM_PLAY_MOVIE(false, 0, 0, 984, true));
			deleteAliveNpcs({831740, 831627, 831741, 831718, 831551, 831552, 831553});
			// Open Cage
			spawn(831598, 522.3982f, 564.6901f, 199.0337f, int8_t{60}, 14);
			// Yume
			spawn(831744, 519.29773f, 565.52289f, 199.72002f, int8_t{60});
			// Nightmare Crate
			spawn(831745, 515.7631f, 577.2256f, 198.7648f, int8_t{0});
			spawn(831745, 515.26715f, 580.4844f, 198.6708f, int8_t{0});
			spawn(831745, 519.28345f, 579.31573f, 198.70723f, int8_t{0});
			spawn(831745, 526.01355f, 583.04895f, 198.75696f, int8_t{0});
			spawn(831745, 531.1339f, 579.4351f, 198.69586f, int8_t{0});
			spawn(831745, 533.6956f, 576.092f, 198.75f, int8_t{0});
			spawn(831745, 535.5213f, 578.81696f, 198.77115f, int8_t{0});
			spawn(831745, 537.1102f, 575.385f, 198.75f, int8_t{0});
			spawn(831745, 537.35767f, 572.5098f, 198.74171f, int8_t{0});
			spawn(831745, 522.6655f, 583.15027f, 198.8219f, int8_t{0});
			spawn(831745, 536.34247f, 550.96124f, 198.875f, int8_t{0});
			spawn(831745, 533.497f, 548.8109f, 198.89424f, int8_t{0});
			spawn(831745, 530.3073f, 551.6777f, 198.66435f, int8_t{0});
			spawn(831745, 525.0943f, 547.747f, 198.875f, int8_t{0});
			spawn(831745, 524.2205f, 551.79425f, 198.75f, int8_t{0});
			spawn(831745, 520.3118f, 549.126f, 198.78514f, int8_t{0});
			// Greater Nightmare Crate
			spawn(831575, 507.42548f, 552.5874f, 199.86153f, int8_t{0});
			spawn(831575, 507.46768f, 556.34448f, 199.86153f, int8_t{0});
			spawn(831575, 507.75433f, 568.13837f, 199.86153f, int8_t{0});
			spawn(831575, 507.77673f, 564.34204f, 199.86153f, int8_t{0});
			spawn(831575, 507.83087f, 571.92407f, 199.86153f, int8_t{0});
			spawn(831575, 507.90619f, 575.8056f, 199.86153f, int8_t{0});
			// Circus Exit
			spawn(831576, 483.58258f, 567.21149f, 201.73489f, int8_t{0});
			// Yume's Friends
			spawn(831560, 520.41559f, 563.30469f, 200.16179f, int8_t{53});
			spawn(831559, 517.784f, 562.0495f, 200.16179f, int8_t{30});
			spawn(831561, 517.84387f, 568.37622f, 200.16179f, int8_t{90});
			spawn(831562, 520.56641f, 567.83276f, 200.16179f, int8_t{73});
			sendMsg(831559, 1501112);
			sendMsg(831561, 1501113);
			sendMsg(831562, 1501121);
			break;
		default:
			break;
	}
}

// Java NightmareCircus.java:343-345
void NightmareCircus::sendMsg(int32_t npcId, int32_t msg) {
	PacketSendUtility::broadcastMessage(getNpc(npcId), msg);
}

// Java NightmareCircus.java:347-354
bool NightmareCircus::onReviveEvent(Player& player) {
	PlayerReviveService::revive(player, 25, 25, false, 0);
	player.getGameStats()->updateStatsAndSpeedVisually();
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	TeleportService::teleportTo(player, *instance, 473.54022f, 567.6342f, 201.83635f, int8_t{118});
	return true;
}

// Java NightmareCircus.java:356-360
void NightmareCircus::onInstanceDestroy() {
	cancelDespawnBossTask();
	isInstanceDestroyed = true;
}

} // namespace aion::gameserver::handlers::instance
