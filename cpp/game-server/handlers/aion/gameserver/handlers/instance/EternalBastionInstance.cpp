#include "aion/gameserver/handlers/instance/EternalBastionInstance.h"

#include <algorithm>
#include <format>

#include "aion/commons/logging/Logger.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/model/gameobjects/StaticDoor.h"
#include "aion/gameserver/model/instance/instancescore/NormalScore.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/network/aion/instanceinfo/EternalBastionScoreWriter.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(EternalBastionInstance, 300540000);

// Java EternalBastionInstance.java:64-66
EternalBastionInstance::EternalBastionInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<EternalBastionInstance> EternalBastionInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<EternalBastionInstance>(instance);
}

// Java EternalBastionInstance.java:68-205
void EternalBastionInstance::onDie(Npc& npc) {
	GeneralInstanceHandler::onDie(npc);
	switch (npc.getNpcId()) {
		case 231168: // Pashid Scout Commander Azute
		case 231169: // Pashid Scout Commander Zest
		case 231170: // Pashid Scout Commander Sartas
		case 231171: // Pashid Infantry Commander Matuk
		case 231172: // Pashid Assault Commander Badute
		case 231173: // Pashid Assault Commander Katsu
		case 231174: // Pashid Artillery Commander Murat
		case 231175: // Pashid Artillery Commander Kaimdu
		case 231176: // Pashid Artillery Commander Nirta
			addPoints(npc, 1880);
			checkProgress(progressionKills.incrementAndGet());
			break;
		case 231143: // Pashid Siege Tower
		case 231152: // Pashid Siege Tower
		case 231153: // Pashid Siege Tower
		case 231154: // Pashid Siege Tower
		case 231155: // Pashid Siege Tower
			addPoints(npc, 334);
			checkProgress(progressionKills.incrementAndGet());
			break;
		case 231177: // Deathbringer Tariksha
			addPoints(npc, 1880);
			break;
		case 231178: // Commander Hakunta
		case 231179: // Commander Rakunta
			addPoints(npc, 1880);
			assaultPower.addAndGet(-2); // Retail
			break;
		case 230784: // Pashid Snare Turret
		case 230785: // Pashid Assault Flamethrower
		case 231137: // Pashid Danuar Turret
		case 231138: // Pashid Danuar Turret
		case 231140: // Pashid Assault Pod
		case 231141: // Pashid Siege Drop Pod
		case 231144: // Pashid Siege Cannon
		case 231156: // Pashid Assault Pod
		case 231157: // Pashid Assault Pod
		case 231158: // Pashid Assault Pod
		case 231159: // Pashid Assault Pod
		case 231160: // Pashid Assault Pod
		case 231162: // Pashid Assault Pod
		case 231163: // Pashid Siege Drop Pod
		case 231164: // Pashid Siege Drop Pod
		case 231165: // Pashid Siege Drop Pod
		case 231167: // Pashid Siege Drop Pod
		case 231180: // Dredgion Signal Tower
			addPoints(npc, 334);
			break;
		case 231148: // Dredgion Signal Tower
			addPoints(npc, 334);
			PacketSendUtility::broadcastToMap(npc, SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5b_TD_AddWave_03());
			spawnWithDelay(231157, 778.845f, 323.282f, 253.434f, int8_t{40}, 30000);
			spawnWithDelay(231159, 697.564f, 305.424f, 249.303f, int8_t{100}, 30000);
			break;
		case 231149: // Pashid Army Barricade
			addPoints(npc, 266);
			PacketSendUtility::broadcastToMap(npc, SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5b_TD_AddWave_02());
			spawnWithDelay(231164, 667.350f, 281.046f, 225.698f, int8_t{33}, 30000); // Pashid Assault Pod
			spawnWithDelay(231165, 721.498f, 358.172f, 230.940f, int8_t{0}, 30000);
			break;
		case 231181: // Pashid Army Barricade
			addPoints(npc, 266);
			break;
		case 230746: // Pashid Assault Tribuni Sentry
		case 230753: // Pashid Assault Rider
		case 230754: // Pashid Assault Gunner
		case 230756: // Pashid Assault Supply Officer
		case 230757: // Pashid Assault Dragon
			addPoints(npc, 1002);
			assaultPower.decrementAndGet();
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5b_TD_Notice_06());
			break;
		case 230744: // Pashid Assault Tribuni Combatant
		case 230745: // Pashid Assault Tribuni Protector
		case 230749: // Pashid Assault Tribuni Marksman
		case 231131: // Pashid Siege Dragon
		case 231132: // Pashid Siege Dragon
		case 231133: // Pashid Siege Dragon
		case 231134: // Pashid Siege Dragon
			addPoints(npc, 1002);
			break;
		case 831333: // Castle Wall
			addPoints(npc, -150);
			deleteAliveNpcs({831332}); // Right Castle Gate
			deleteAliveNpcs({231150}); // Drill
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5b_TD_Notice_04());
			break;
		case 831335: // Inner Water Gate
			addPoints(npc, -150);
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5b_TD_Notice_02());
			break;
		case 209516: // Commander Lysander
		case 209517: // Commander Granir
			addPoints(npc, -100000); // Retail
			endInstance();
			break;
		case 209555: // Lysander's Disciple
		case 209557: // Granir's Disciple
			addPoints(npc, -50);
			break;
		case 231130: // Grand Commander Pashid
			addPoints(npc, 24000);
			endInstance();
			break;
		case 231117: // Pashid Elite Siege Combatant
		case 231118: // Pashid Elite Siege Protector
		case 231119: // Pashid Elite Siege Ambusher
		case 231120: // Pashid Elite Siege Troublemaker
		case 231122: // Pashid Elite Siege Marksman
		case 231123: // Pashid Elite Siege Rampager
		case 231124: // Pashid Elite Siege Magus
		case 231125: // Pashid Elite Siege Summoner
		case 231126: // Pashid Elite Siege Cavalry
		case 231127: // Pashid Elite Siege Striker
		case 231128: // Pashid Elite Siege Medic
		case 233310: // Pashid Siege Cavalry
		case 233311: // Pashid Siege Engineer
			addPoints(npc, 42);
			break;
		case 233312: // Pashid Siege Healer
		case 233314: // Pashid Elite Siege Defender
		case 233315: // Pashid Elite Siege Gunner
			addPoints(npc, 36);
			break;
		case 231115: // Pashid Siege Soldier
		case 231116: // Pashid Siege Mage
		case 233309: // Pashid Siege Ambusher
			addPoints(npc, 33);
			break;
		case 233313:
			addPoints(npc, 20);
			break;
	}
}

// Java EternalBastionInstance.java:207-281
void EternalBastionInstance::checkProgress(int32_t kills) { // Java: progressionKills (it hides the member)
	switch (kills) {
		case 3: {
			runtime::Ptr<Npc> outerWaterGate = getNpc(831334);
			if (outerWaterGate != nullptr)
				outerWaterGate->getController().deleteIfAliveOrCancelRespawn();
			spawn(233314, 575.858f, 146.753f, 221.351f, int8_t{33}); // Pashid Elite Siege Defender
			spawn(233314, 587.445f, 152.020f, 218.004f, int8_t{63});
			spawn(233314, 609.691f, 187.747f, 216.455f, int8_t{87});
			spawn(233314, 630.440f, 192.271f, 219.763f, int8_t{40});
			spawn(233315, 598.051f, 160.956f, 216.754f, int8_t{100}); // Pashid Elite Siege Gunner
			spawn(233315, 609.099f, 150.973f, 216.063f, int8_t{57});
			spawn(233315, 637.820f, 203.284f, 222.032f, int8_t{77});
			spawn(233315, 641.959f, 197.833f, 221.788f, int8_t{77});

			spawnTasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] {
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5b_TD_MainWave_02());
				spawn(231171, 655.755f, 212.606f, 223.931f, int8_t{80}); // Pashid Infantry Commander Matuk
				spawnWithWalker(231142, 604.397f, 170.492f, 216.042f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_Z1_S2_D1"); // Pashid Siege Volatile
				spawnWithWalker(231142, 605.397f, 171.492f, 216.092f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_Z1_S2_D1");
				spawnWithWalker(231142, 603.397f, 171.492f, 216.085f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_Z1_S2_D1");
				spawnWithWalker(231173, 657.052f, 465.173f, 225.052f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_Z1_S2_B2F2"); // Pashid Assault Commander Katsu
				spawnWithWalker(233313, 659.052f, 467.173f, 225.000f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_Z1_S2_B2F2");
				spawnWithWalker(233313, 655.052f, 467.173f, 225.133f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_Z1_S2_B2F2");
				spawnWithWalker(231172, 604.429f, 413.910f, 223.782f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_Z1_S2_B1F2"); // Pashid Assault Commander Badute
				spawnWithWalker(233313, 606.429f, 411.910f, 224.027f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_Z1_S2_B1F2");
				spawnWithWalker(233313, 602.429f, 411.910f, 223.756f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_Z1_S2_B1F2");
			}, 90 * 1000));
			break;
		}
		case 6:
			spawnTasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] {
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5b_TD_MainWave_03());
				spawn(233313, 572.428f, 368.118f, 226.464f, int8_t{113}); // Pashid Siege Combatant
				spawn(233313, 577.691f, 374.779f, 226.077f, int8_t{110});
				spawn(233313, 583.372f, 380.359f, 225.562f, int8_t{107});
				spawn(233313, 590.788f, 386.729f, 224.273f, int8_t{100});
				spawn(233313, 652.680f, 456.840f, 225.698f, int8_t{110});
				spawn(233313, 660.402f, 469.521f, 225.095f, int8_t{113});
				spawn(233313, 670.701f, 477.320f, 225.120f, int8_t{100});
				spawn(233313, 681.626f, 481.653f, 224.853f, int8_t{100});
				spawn(231137, 569.389f, 374.023f, 228.221f, int8_t{110}); // Pashid Danuar Turret
				spawn(231137, 576.424f, 381.682f, 226.099f, int8_t{107});
				spawn(231137, 584.247f, 388.219f, 225.080f, int8_t{103});
				spawn(231138, 650.886f, 466.252f, 225.282f, int8_t{110});
				spawn(231138, 661.941f, 478.229f, 226.286f, int8_t{103});
				spawn(231138, 673.506f, 486.307f, 225.869f, int8_t{100});
				spawn(231140, 635.426f, 243.117f, 238.075f, int8_t{33}); // Pashid Assault Pods
				spawn(231141, 666.361f, 294.435f, 225.698f, int8_t{20});
				spawn(231158, 768.339f, 390.709f, 243.356f, int8_t{40});
				spawn(231174, 669.851f, 468.267f, 225.250f, int8_t{107}); // Pashid Artillery Commander Murat
				spawn(231175, 583.830f, 373.812f, 225.280f, int8_t{107}); // Pashid Artillery Commander Kaimdu
				spawn(231176, 760.219f, 392.471f, 243.354f, int8_t{50}); // Pashid Infantry Commander Nirta
			}, 90 * 1000));
			break;
		case 9:
			spawnTasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] {
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5b_TD_MainWave_04());
				spawn(231143, 613.231f, 262.163f, 227.255f, int8_t{3});
				spawn(231152, 608.371f, 303.514f, 226.295f, int8_t{113});
				spawn(231153, 625.244f, 352.624f, 226.295f, int8_t{113});
				spawn(231154, 668.864f, 405.970f, 228.500f, int8_t{83});
				spawn(231155, 691.536f, 409.367f, 231.720f, int8_t{98});
			}, 90 * 1000));
			break;
		case 14:
			spawnTasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] {
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5b_TD_MainWave_05());
				spawn(231130, 740.668f, 298.082f, 233.889f, int8_t{100}); // Commander Pashid
				spawn(231131, 686.574f, 358.216f, 243.386f, int8_t{100}); // Pashid Siege Dragons
				spawn(231131, 655.856f, 351.118f, 241.595f, int8_t{20});
				spawn(231131, 732.982f, 371.320f, 230.942f, int8_t{106});
				spawn(231132, 582.631f, 376.172f, 225.461f, int8_t{100});
				spawn(231133, 745.820f, 322.916f, 249.287f, int8_t{86});
				spawn(231133, 713.242f, 289.971f, 249.285f, int8_t{0});
				spawn(231134, 668.732f, 473.705f, 225.159f, int8_t{100});
				spawn(231156, 641.551f, 339.264f, 238.075f, int8_t{20}); // Pashid Assault Pods
				spawn(231163, 727.175f, 364.431f, 230.941f, int8_t{7});
			}, 90 * 1000));
			break;
	}
}

// Java EternalBastionInstance.java:283-301
void EternalBastionInstance::onEnterInstance(Player& player) {
	if (!instanceReward.get()->isRewarded())
		sendPacket();
	if (isRaceSet.compareAndSet(false, true)) {
		spawnRaceGuards(player.getRace());
		if (commons::utils::Rnd::nextBoolean()) {
			spawn(231177, 821.146f, 607.305f, 239.703f, int8_t{73}); // Deathbringer Tariksha
			spawn(230746, 551.146f, 412.105f, 222.760f, int8_t{30}); // Pashid Assault Tribuni Sentry
			spawn(231149, 702.116f, 552.614f, 232.423f, int8_t{110}); // Pashid Army Barricade (Assault Pod Trigger)
			spawn(231181, 564.414f, 250.835f, 233.198f, int8_t{110}); // Pashid Army Barricade
		} else {
			spawn(230746, 821.146f, 607.305f, 239.703f, int8_t{73}); // Pashid Assault Tribuni Sentry
			spawn(231177, 551.146f, 412.105f, 222.760f, int8_t{30}); // Deathbringer Tariksha
			spawn(231181, 702.116f, 552.614f, 232.423f, int8_t{110}); // Pashid Army Barricade
			spawn(231149, 564.414f, 250.835f, 233.198f, int8_t{110}); // Pashid Army Barricade (Assault Pod Trigger)
		}
	}
}

// Java EternalBastionInstance.java:303-351
void EternalBastionInstance::spawnRaceGuards(Race race) {
	int32_t guardId = race == Race::ELYOS ? 209555 : 209557;
	spawn(race == Race::ELYOS ? 209516 : 209517, 750.205f, 285.880f, 233.752f, int8_t{40}); // Commander
	spawn(race == Race::ELYOS ? 701923 : 701924, 744.174f, 292.949f, 233.698f, int8_t{40}); // Flag
	spawn(race == Race::ELYOS ? 701625 : 701922, 640.862f, 412.784f, 243.940f, int8_t{40}); // Siege Cannon
	spawn(guardId, 595.476f, 284.680f, 226.375f, int8_t{40});
	spawn(guardId, 598.868f, 284.201f, 226.424f, int8_t{40});
	spawn(guardId, 602.328f, 340.964f, 225.794f, int8_t{40});
	spawn(guardId, 605.731f, 343.153f, 225.448f, int8_t{40});
	spawn(guardId, 607.450f, 387.642f, 223.353f, int8_t{40});
	spawn(guardId, 611.817f, 388.865f, 223.500f, int8_t{40});
	spawn(guardId, 681.742f, 444.580f, 226.818f, int8_t{40});
	spawn(guardId, 684.437f, 447.848f, 226.787f, int8_t{40});
	spawn(guardId, 690.046f, 351.800f, 244.744f, int8_t{40});
	spawn(guardId, 690.220f, 341.532f, 228.674f, int8_t{40});
	spawn(guardId, 692.778f, 337.952f, 228.674f, int8_t{40});
	spawn(guardId, 693.082f, 354.432f, 244.733f, int8_t{40});
	spawn(guardId, 715.405f, 427.312f, 230.025f, int8_t{40});
	spawn(guardId, 719.378f, 428.101f, 230.112f, int8_t{40});
	spawn(guardId, 748.146f, 361.345f, 230.945f, int8_t{40});
	spawn(guardId, 749.389f, 364.988f, 230.945f, int8_t{40});
	if (race == Race::ELYOS) {
		spawn(701596, 617.501f, 248.196f, 235.740f, int8_t{60}); // Cannons
		spawn(701597, 612.806f, 275.206f, 235.740f, int8_t{67});
		spawn(701598, 616.159f, 313.939f, 235.740f, int8_t{53});
		spawn(701599, 625.603f, 339.608f, 235.734f, int8_t{53});
		spawn(701600, 650.914f, 372.932f, 238.607f, int8_t{53});
		spawn(701601, 677.853f, 396.203f, 238.632f, int8_t{40});
		spawn(701602, 710.145f, 410.661f, 241.014f, int8_t{30});
		spawn(701603, 736.803f, 414.121f, 241.017f, int8_t{40});
		spawn(701604, 772.961f, 410.834f, 241.014f, int8_t{20});
		spawn(701605, 798.383f, 401.605f, 241.015f, int8_t{30});
		spawn(701606, 709.602f, 313.531f, 254.216f, int8_t{40});
		spawn(701607, 726.757f, 327.932f, 254.216f, int8_t{50});
	} else {
		spawn(701610, 617.501f, 248.196f, 235.740f, int8_t{60}); // Cannons
		spawn(701611, 612.806f, 275.206f, 235.740f, int8_t{67});
		spawn(701612, 616.159f, 313.939f, 235.740f, int8_t{53});
		spawn(701613, 625.603f, 339.608f, 235.734f, int8_t{53});
		spawn(701614, 650.914f, 372.932f, 238.607f, int8_t{53});
		spawn(701615, 677.853f, 396.203f, 238.632f, int8_t{40});
		spawn(701616, 710.145f, 410.661f, 241.014f, int8_t{30});
		spawn(701617, 736.803f, 414.121f, 241.017f, int8_t{40});
		spawn(701618, 772.961f, 410.834f, 241.014f, int8_t{20});
		spawn(701619, 798.383f, 401.605f, 241.015f, int8_t{30});
		spawn(701620, 709.602f, 313.531f, 254.216f, int8_t{40});
		spawn(701621, 726.757f, 327.932f, 254.216f, int8_t{50});
	}
}

// Java EternalBastionInstance.java:353-360
void EternalBastionInstance::onInstanceCreate() {
	instanceReward = NormalScore::create();
	instanceReward.get()->setInstanceProgressionType(InstanceProgressionType::PREPARING);
	instanceReward.get()->setPoints(20000);
	startTime = commons::utils::currentTimeMillis();
	instanceTimerTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { onStart(); }, START_DELAY));
}

// Java EternalBastionInstance.java:362-389
void EternalBastionInstance::onStart() {
	startTime = commons::utils::currentTimeMillis();
	instanceReward.get()->setInstanceProgressionType(InstanceProgressionType::START_PROGRESS);
	sendPacket();
	instance->forEachDoor([](model::gameobjects::StaticDoor& door) { door.setOpen(true); });
	assaultWaveTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { spawnAssaultWave(); }, 60000, 60000));
	instanceTimerTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { onTimeOut(); }, 30 * 60 * 1000));

	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5b_TD_MainWave_01());
	spawn(233313, 584.013f, 371.221f, 225.374f, int8_t{110}); // Pashid Siege Fighter
	spawn(233313, 588.725f, 377.543f, 225.221f, int8_t{110});
	spawn(233313, 655.190f, 454.515f, 225.936f, int8_t{110});
	spawn(233313, 659.293f, 461.406f, 225.449f, int8_t{110});
	spawn(233313, 795.178f, 462.909f, 225.853f, int8_t{118});
	spawn(233313, 804.449f, 461.860f, 227.897f, int8_t{58});
	spawn(233315, 572.093f, 377.641f, 227.147f, int8_t{110}); // Pashid Elite Siege Gunner
	spawn(233315, 580.561f, 387.814f, 225.668f, int8_t{110});
	spawn(233315, 646.597f, 458.471f, 225.575f, int8_t{117});
	spawn(233315, 652.617f, 467.432f, 225.265f, int8_t{113});
	spawn(233315, 794.179f, 474.019f, 225.361f, int8_t{88});
	spawn(233315, 806.574f, 473.837f, 227.837f, int8_t{98});
	spawn(231168, 652.191f, 461.264f, 225.095f, int8_t{110}); // Pashid Scout Commander Azute
	spawn(231169, 581.777f, 377.664f, 225.528f, int8_t{110}); // Pashid Scout Commander Zest
	spawn(231170, 800.515f, 469.416f, 228.586f, int8_t{88}); // Pashid Scout Commander Sartas
	spawn(831334, 569.772f, 162.763f, 220.048f, int8_t{53}, 271); // Outer Water Gate
	spawnWithDelay(231167, 735.282f, 295.307f, 233.752f, int8_t{115}, 9000); // Pashid Assault Pods
	spawnWithDelay(231162, 747.273f, 300.182f, 233.752f, int8_t{97}, 6000);
}

// Java EternalBastionInstance.java:391-530
void EternalBastionInstance::spawnAssaultWave() {
	waveCount = waveCount.get() + 1;
	switch (waveCount.get()) {
		case 1:
		case 5:
		case 13:
		case 17:
		case 25:
			spawnAssaultPodWave();
			break;
		case 2:
			spawnEasternWaveOne();
			break;
		case 4: {
			spawnEasternWaveOne();
			spawnNorthernWaveOne();
			break;
		}
		case 6: {
			spawnEasternWaveOne();
			spawnNorthernWaveOne();
			spawnEasternWaveTwo();
			spawnCanalWave();
			spawnSiegeTowerWave();
			break;
		}
		case 8: {
			spawnEasternWaveOne();
			spawnNorthernWaveOne();
			spawnEasternWaveTwo();
			spawnWesternWave();
			break;
		}
		case 9:
		case 21: {
			spawnAssaultPodWave();
			spawnSiegeTowerWave();
			break;
		}
		case 10: {
			spawnEasternWaveOne();
			spawnNorthernWaveOne();
			spawnEasternWaveTwo();
			spawnWesternWave();
			spawnNorthernWaveTwo();
			spawnCanalWave();
			if (assaultPower.get() >= 8) {
				spawnWithWalker(231142, 795.579f, 478.629f, 225.086f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S38"); // Pashid Siege Volatile
				spawnWithWalker(231142, 798.579f, 479.629f, 225.221f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S38");
				spawnWithWalker(231142, 792.579f, 479.629f, 224.934f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S38");
				spawnWithWalker(231142, 801.579f, 481.629f, 225.845f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S38");
				spawnWithWalker(231142, 789.579f, 481.629f, 224.622f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S38");
			}
			break;
		}
		case 12: {
			spawnEasternWaveOne();
			spawnNorthernWaveOne();
			spawnEasternWaveTwo();
			spawnWesternWave();
			spawnNorthernWaveTwo();
			spawnEasternWaveThree();
			spawnSiegeTowerWave();
			break;
		}
		case 14: {
			spawnEasternWaveOne();
			spawnNorthernWaveOne();
			spawnEasternWaveTwo();
			spawnWesternWave();
			spawnNorthernWaveTwo();
			spawnEasternWaveThree();
			spawnCanalWave();
			break;
		}
		case 15: {
			spawnCanalWave();
			spawnSiegeTowerWave();
			if (assaultPower.get() >= 7 && getNpc(831333) != nullptr)
				spawnWithWalker(231150, 798.563f, 477.952f, 225.231f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S73"); // Pashid Siege Ram
			break;
		}
		case 16: {
			if (assaultPower.get() >= 11)
				spawnEasternWaveOne();
			spawnNorthernWaveOne();
			spawnEasternWaveTwo();
			if (assaultPower.get() >= 12)
				spawnWesternWave();
			spawnNorthernWaveTwo();
			spawnEasternWaveThree();
			break;
		}
		case 18: {
			spawnEasternWaveOne();
			spawnNorthernWaveOne();
			if (assaultPower.get() >= 10)
				spawnEasternWaveTwo();
			spawnWesternWave();
			if (assaultPower.get() >= 9)
				spawnNorthernWaveTwo();
			spawnEasternWaveThree();
			spawnCanalWave();
			spawnSouthernWave();
			spawnSiegeTowerWave();
			break;
		}
		case 20: {
			spawnEasternWaveOne();
			spawnNorthernWaveOne();
			spawnEasternWaveTwo();
			spawnWesternWave();
			spawnNorthernWaveTwo();
			spawnEasternWaveThree();
			break;
		}
		case 22: {
			spawnEasternWaveOne();
			spawnNorthernWaveOne();
			spawnEasternWaveTwo();
			spawnWesternWave();
			spawnNorthernWaveTwo();
			spawnEasternWaveThree();
			spawnCanalWave();
			spawnSouthernWave();
			break;
		}
		case 24: {
			if (assaultPower.get() >= 5)
				spawnEasternWaveOne();
			spawnNorthernWaveOne();
			spawnEasternWaveTwo();
			if (assaultPower.get() >= 6)
				spawnWesternWave();
			spawnNorthernWaveTwo();
			spawnEasternWaveThree();
			spawnSiegeTowerWave();
			break;
		}
		case 26: {
			spawnEasternWaveOne();
			if (assaultPower.get() >= 3)
				spawnNorthernWaveOne();
			spawnEasternWaveTwo();
			spawnWesternWave();
			spawnNorthernWaveTwo();
			if (assaultPower.get() >= 4)
				spawnEasternWaveThree();
			spawnCanalWave();
			spawnSouthernWave();
			break;
		}
		case 28: {
			spawnEasternWaveOne();
			spawnNorthernWaveOne();
			if (assaultPower.get() >= 1)
				spawnEasternWaveTwo();
			spawnWesternWave();
			if (assaultPower.get() >= 2)
				spawnNorthernWaveTwo();
			spawnEasternWaveThree();
			break;
		}
	}
}

// Java EternalBastionInstance.java:532-536
void EternalBastionInstance::spawnEasternWaveOne() {
	spawnWithWalker(231113, 652.071f, 475.738f, 226.125f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S46"); // East 1
	spawnWithWalker(231110, 655.071f, 478.738f, 226.125f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S46");
	spawnWithWalker(231110, 649.071f, 478.738f, 226.125f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S46");
}

// Java EternalBastionInstance.java:538-542
void EternalBastionInstance::spawnEasternWaveTwo() {
	spawnWithWalker(231114, 671.857f, 480.417f, 225.195f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S32"); // East 2
	spawnWithWalker(231112, 674.857f, 483.417f, 225.337f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S32");
	spawnWithWalker(231112, 668.857f, 483.417f, 226.457f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S32");
}

// Java EternalBastionInstance.java:544-548
void EternalBastionInstance::spawnEasternWaveThree() {
	spawnWithWalker(231113, 632.525f, 451.311f, 223.422f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S34"); // East 3
	spawnWithWalker(231111, 635.525f, 454.311f, 223.193f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S34");
	spawnWithWalker(231111, 629.525f, 454.311f, 220.445f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S34");
}

// Java EternalBastionInstance.java:550-554
void EternalBastionInstance::spawnNorthernWaveOne() {
	spawnWithWalker(231113, 598.026f, 411.715f, 223.784f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S71"); // North 1
	spawnWithWalker(231110, 601.026f, 414.715f, 223.519f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S71");
	spawnWithWalker(231110, 595.026f, 414.715f, 223.552f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S71");
}

// Java EternalBastionInstance.java:556-560
void EternalBastionInstance::spawnNorthernWaveTwo() {
	spawnWithWalker(231113, 569.237f, 387.007f, 227.533f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S36"); // North 2
	spawnWithWalker(231111, 572.237f, 390.007f, 227.905f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S36");
	spawnWithWalker(231111, 566.237f, 390.007f, 228.194f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S36");
}

// Java EternalBastionInstance.java:562-566
void EternalBastionInstance::spawnWesternWave() {
	spawnWithWalker(231114, 587.952f, 239.621f, 229.530f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S44"); // West
	spawnWithWalker(231112, 590.952f, 242.621f, 229.152f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S44");
	spawnWithWalker(231112, 584.952f, 242.621f, 229.822f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S44");
}

// Java EternalBastionInstance.java:568-574
void EternalBastionInstance::spawnSouthernWave() {
	if (getNpc(831333) == nullptr) {
		spawnWithWalker(231113, 794.134f, 483.021f, 224.756f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S39"); // South Wall
		spawnWithWalker(231113, 796.134f, 481.021f, 225.008f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S39");
		spawnWithWalker(231113, 792.134f, 481.021f, 224.820f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_S39");
	}
}

// Java EternalBastionInstance.java:576-582
void EternalBastionInstance::spawnCanalWave() {
	if (getNpc(831335) == nullptr) {
		spawnWithWalker(231110, 610.571f, 189.724f, 216.509f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_Z1_S2_T1"); // Canal
		spawnWithWalker(231108, 612.571f, 191.724f, 216.589f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_Z1_S2_T1");
		spawnWithWalker(231108, 608.571f, 187.724f, 216.574f, int8_t{100}, "NPCPathIDLDF5b_TD_Mob_Z1_S2_T1");
	}
}

// Java EternalBastionInstance.java:584-635
void EternalBastionInstance::spawnAssaultPodWave() {
	if (getNpc(231140) != nullptr) {
		spawnWithWalker(231106, 633.457f, 245.792f, 238.075f, int8_t{33}, "NPCPathIDLDF5b_TD_Mob_Z1_S3_POD01");
		spawnWithWalker(231108, 635.457f, 247.792f, 238.075f, int8_t{33}, "NPCPathIDLDF5b_TD_Mob_Z1_S3_POD01");
		spawnWithWalker(231108, 631.457f, 247.792f, 238.075f, int8_t{33}, "NPCPathIDLDF5b_TD_Mob_Z1_S3_POD01");
	}
	if (getNpc(231156) != nullptr) {
		spawnWithWalker(231106, 642.871f, 343.420f, 238.075f, int8_t{20}, "NPCPathIDLDF5b_TD_Z1_S5_POD01");
		spawnWithWalker(231108, 644.871f, 345.420f, 238.075f, int8_t{20}, "NPCPathIDLDF5b_TD_Z1_S5_POD01");
		spawnWithWalker(231108, 640.871f, 345.420f, 238.075f, int8_t{20}, "NPCPathIDLDF5b_TD_Z1_S5_POD01");
	}
	if (getNpc(231157) != nullptr) {
		spawnWithWalker(231106, 776.242f, 326.041f, 253.434f, int8_t{40}, "NPCPathIDLDF5b_TD_Z4_POD02");
		spawnWithWalker(231108, 778.242f, 328.041f, 253.434f, int8_t{40}, "NPCPathIDLDF5b_TD_Z4_POD02");
		spawnWithWalker(231108, 774.242f, 328.041f, 253.434f, int8_t{40}, "NPCPathIDLDF5b_TD_Z4_POD02");
	}
	if (getNpc(231158) != nullptr) {
		spawnWithWalker(231106, 765.481f, 393.614f, 243.354f, int8_t{40}, "NPCPathIDLDF5b_TD_Mob_Z1_S3_POD3");
		spawnWithWalker(231108, 767.481f, 395.614f, 243.354f, int8_t{40}, "NPCPathIDLDF5b_TD_Mob_Z1_S3_POD3");
		spawnWithWalker(231108, 763.481f, 395.614f, 243.354f, int8_t{40}, "NPCPathIDLDF5b_TD_Mob_Z1_S3_POD3");
	}
	if (getNpc(231141) != nullptr) {
		spawnWithWalker(231105, 667.631f, 297.565f, 225.700f, int8_t{20}, "NPCPathIDLDF5b_TD_Mob_Z1_S3_POD2");
		spawnWithWalker(231107, 669.631f, 299.565f, 225.700f, int8_t{20}, "NPCPathIDLDF5b_TD_Mob_Z1_S3_POD2");
		spawnWithWalker(231107, 665.631f, 299.565f, 225.700f, int8_t{20}, "NPCPathIDLDF5b_TD_Mob_Z1_S3_POD2");
	}
	if (getNpc(231163) != nullptr) {
		spawnWithWalker(231105, 731.089f, 365.461f, 230.941f, int8_t{7}, "NPCPathIDLDF5b_TD_Z1_S5_POD02");
		spawnWithWalker(231107, 731.089f, 365.461f, 230.941f, int8_t{7}, "NPCPathIDLDF5b_TD_Z1_S5_POD02");
		spawnWithWalker(231107, 731.089f, 365.461f, 230.941f, int8_t{7}, "NPCPathIDLDF5b_TD_Z1_S5_POD02");
	}
	if (getNpc(231159) != nullptr) {
		spawnWithWalker(231106, 699.760f, 302.938f, 249.303f, int8_t{100}, "NPCPathIDLDF5b_TD_Z4_POD01");
		spawnWithWalker(231108, 701.760f, 304.938f, 249.303f, int8_t{100}, "NPCPathIDLDF5b_TD_Z4_POD01");
		spawnWithWalker(231108, 697.760f, 304.938f, 249.303f, int8_t{100}, "NPCPathIDLDF5b_TD_Z4_POD01");
	}
	if (getNpc(231162) != nullptr) { // Could be a bug on retail, but anyway
		spawnWithWalker(231106, 724.927f, 359.346f, 230.941f, int8_t{0}, "NPCPathIDLDF5b_TD_Z3_POD02");
		spawnWithWalker(231108, 726.927f, 361.346f, 230.941f, int8_t{0}, "NPCPathIDLDF5b_TD_Z3_POD02");
		spawnWithWalker(231108, 722.927f, 361.346f, 230.941f, int8_t{0}, "NPCPathIDLDF5b_TD_Z3_POD02");
	}
	if (getNpc(231164) != nullptr) {
		spawnWithWalker(231106, 724.927f, 359.346f, 230.941f, int8_t{0}, "NPCPathIDLDF5b_TD_Z3_POD02");
		spawnWithWalker(231108, 726.927f, 361.346f, 230.941f, int8_t{0}, "NPCPathIDLDF5b_TD_Z3_POD02");
		spawnWithWalker(231108, 722.927f, 361.346f, 230.941f, int8_t{0}, "NPCPathIDLDF5b_TD_Z3_POD02");
	}
	if (getNpc(231165) != nullptr) {
		spawnWithWalker(231106, 724.927f, 359.346f, 230.941f, int8_t{0}, "NPCPathIDLDF5b_TD_Z3_POD02");
		spawnWithWalker(231108, 726.927f, 361.346f, 230.941f, int8_t{0}, "NPCPathIDLDF5b_TD_Z3_POD02");
		spawnWithWalker(231108, 722.927f, 361.346f, 230.941f, int8_t{0}, "NPCPathIDLDF5b_TD_Z3_POD02");
	}
}

// Java EternalBastionInstance.java:637-663
void EternalBastionInstance::spawnSiegeTowerWave() {
	if (getNpc(230783) != nullptr) {
		spawnWithWalker(231107, 623.235f, 263.392f, 238.484f, int8_t{3}, "NPCPathIDLDF5b_TD_Z1_S4_T1");
		spawnWithWalker(231105, 625.235f, 265.392f, 238.484f, int8_t{3}, "NPCPathIDLDF5b_TD_Z1_S4_T1");
		spawnWithWalker(231105, 621.235f, 265.392f, 238.484f, int8_t{3}, "NPCPathIDLDF5b_TD_Z1_S4_T1");
	}
	if (getNpc(231152) != nullptr) {
		spawnWithWalker(231107, 621.920f, 298.179f, 238.075f, int8_t{113}, "NPCPathIDLDF5b_TD_Z1_S4_T2");
		spawnWithWalker(231105, 623.920f, 300.179f, 238.075f, int8_t{113}, "NPCPathIDLDF5b_TD_Z1_S4_T2");
		spawnWithWalker(231105, 619.920f, 300.179f, 238.075f, int8_t{113}, "NPCPathIDLDF5b_TD_Z1_S4_T2");
	}
	if (getNpc(231153) != nullptr) {
		spawnWithWalker(231107, 644.089f, 351.522f, 239.764f, int8_t{113}, "NPCPathIDLDF5b_TD_Z1_S4_T3");
		spawnWithWalker(231105, 646.089f, 353.522f, 241.151f, int8_t{113}, "NPCPathIDLDF5b_TD_Z1_S4_T3");
		spawnWithWalker(231105, 642.089f, 353.522f, 239.809f, int8_t{113}, "NPCPathIDLDF5b_TD_Z1_S4_T3");
	}
	if (getNpc(231154) != nullptr) {
		spawnWithWalker(231107, 664.091f, 394.303f, 240.223f, int8_t{83}, "NPCPathIDLDF5b_TD_Z1_S4_T4");
		spawnWithWalker(231105, 666.091f, 396.303f, 240.223f, int8_t{83}, "NPCPathIDLDF5b_TD_Z1_S4_T4");
		spawnWithWalker(231105, 662.091f, 396.303f, 240.223f, int8_t{83}, "NPCPathIDLDF5b_TD_Z1_S4_T4");
	}
	if (getNpc(231155) != nullptr) {
		spawnWithWalker(231107, 692.867f, 396.708f, 241.594f, int8_t{85}, "NPCPathIDLDF5b_TD_Z1_S4_T5");
		spawnWithWalker(231105, 694.867f, 398.708f, 242.018f, int8_t{85}, "NPCPathIDLDF5b_TD_Z1_S4_T5");
		spawnWithWalker(231105, 690.867f, 398.708f, 241.594f, int8_t{85}, "NPCPathIDLDF5b_TD_Z1_S4_T5");
	}
}

// Java EternalBastionInstance.java:665-668
void EternalBastionInstance::onTimeOut() {
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5b_TD_MainWave_06());
	endInstance();
}

// Java EternalBastionInstance.java:678-728 (the original points for the ranks: 92,000 S, 84,000 A, 76,000 B, 50,000 C, 10,000 D)
void EternalBastionInstance::endInstance() {
	cancelTasks();
	instanceReward.get()->setInstanceProgressionType(InstanceProgressionType::END_PROGRESS);

	int rank = getFinalRank();
	switch (rank) {
		case 1: {
			instanceReward.get()->setFinalAp(35000);
			instanceReward.get()->setRewardItem1(186000242); // Ceramium Medal
			instanceReward.get()->setRewardItem1Count(4);
			instanceReward.get()->setRewardItem2(188052596); // Highest Grade Material Support Bundle
			instanceReward.get()->setRewardItem2Count(1);
			instanceReward.get()->setRewardItem3(188052594); // Highest Grade Material Box
			instanceReward.get()->setRewardItem3Count(1);
			break;
		}
		case 2: {
			instanceReward.get()->setFinalAp(25000);
			instanceReward.get()->setRewardItem1(186000242); // Ceramium Medal
			instanceReward.get()->setRewardItem1Count(2);
			instanceReward.get()->setRewardItem2(188052594); // Highest Grade Material Box
			instanceReward.get()->setRewardItem2Count(1);
			instanceReward.get()->setRewardItem3(188052597); // High Grade Material Support Bundle
			instanceReward.get()->setRewardItem3Count(1);
			break;
		}
		case 3: { // B-Rank
			instanceReward.get()->setFinalAp(15000);
			instanceReward.get()->setRewardItem1(186000242); // Ceramium Medal
			instanceReward.get()->setRewardItem1Count(1);
			instanceReward.get()->setRewardItem2(188052595); // High Grade Material Box
			instanceReward.get()->setRewardItem2Count(1);
			instanceReward.get()->setRewardItem3(188052598); // Low Grade Material Support Bundle
			instanceReward.get()->setRewardItem3Count(1);
			break;
		}
		case 4: { // C-Rank
			instanceReward.get()->setFinalAp(11000);
			instanceReward.get()->setRewardItem1(188052598); // Low Grade Material Support Bundle
			instanceReward.get()->setRewardItem1Count(1);
			break;
		}
		case 5:
			instanceReward.get()->setFinalAp(7000); // D-Rank
			break;
	}
	instanceReward.get()->setInstanceProgressionType(InstanceProgressionType::END_PROGRESS);
	instanceReward.get()->setRank(rank);
	instance->forEachNpc([](Npc& npc) { npc.getController().delete_(); });
	sendPacket();
	instance->forEachPlayer([this](Player& player) { distributeRewards(player); });
	spawnFinalChest(rank);
	spawn(730871, 766.458f, 263.157f, 233.498f, int8_t{100}); // Exit
	std::string players; // Java: Collectors.joining(", ") of String.format("%s (ID:%d)", name, objectId)
	for (const runtime::Ptr<Player>& p : instance->getPlayersInside()) {
		if (!players.empty())
			players += ", ";
		players += std::format("{} (ID:{})", p->getName(), p->getObjectId());
	}
	log.info("[{}] Instance completed with {} points resulting in {}-Rank. Player(s) in instance: {}",
		dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(mapId)->getName(), instanceReward.get()->getPoints(), getRankNameById(rank), players);
}

// Java EternalBastionInstance.java:730-744
int32_t EternalBastionInstance::getFinalRank() {
	if (instanceReward.get()->getPoints() >= 90000) { // S-Rank
		return 1;
	} else if (instanceReward.get()->getPoints() >= 82000) { // A-Rank
		return 2;
	} else if (instanceReward.get()->getPoints() >= 60000) { // B-Rank
		return 3;
	} else if (instanceReward.get()->getPoints() >= 30000) { // C-Rank
		return 4;
	} else if (instanceReward.get()->getPoints() >= 5000) { // D-Rank
		return 5;
	} else {
		return 8;
	}
}

// Java EternalBastionInstance.java:746-754
void EternalBastionInstance::spawnFinalChest(int32_t rank) {
	switch (rank) {
		case 1:
			spawn(701913, 744.167f, 292.860f, 233.702f, int8_t{100}); // Biggest in model size
			break;
		case 2:
			spawn(701914, 744.167f, 292.860f, 233.702f, int8_t{100});
			break;
		case 3:
			spawn(701915, 744.167f, 292.860f, 233.702f, int8_t{100});
			break;
		case 4:
			spawn(701916, 744.167f, 292.860f, 233.702f, int8_t{100});
			break;
		case 5:
			spawn(701917, 744.167f, 292.860f, 233.702f, int8_t{100}); // Smallest in model size
			break;
	}
}

// Java EternalBastionInstance.java:756-766
void EternalBastionInstance::distributeRewards(Player& player) {
	AbyssPointsService::addAp(player, instanceReward.get()->getFinalAp());
	if (instanceReward.get()->getRewardItem1() > 0)
		ItemService::addItem(player, instanceReward.get()->getRewardItem1(), instanceReward.get()->getRewardItem1Count(), true);
	if (instanceReward.get()->getRewardItem2() > 0)
		ItemService::addItem(player, instanceReward.get()->getRewardItem2(), instanceReward.get()->getRewardItem2Count(), true);
	if (instanceReward.get()->getRewardItem3() > 0)
		ItemService::addItem(player, instanceReward.get()->getRewardItem3(), instanceReward.get()->getRewardItem3Count(), true);
	if (instanceReward.get()->getRewardItem4() > 0)
		ItemService::addItem(player, instanceReward.get()->getRewardItem4(), instanceReward.get()->getRewardItem4Count(), true);
}

// Java EternalBastionInstance.java:768-776
void EternalBastionInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	if (npc.getNpcId() == 701625 || npc.getNpcId() == 701922) {
		SkillEngine::getInstance().getSkill(npc, 21069, 1, runtime::Ptr<VisibleObject>(npc))->useSkill();
		runtime::Ref<Npc> cannon(npc);
		ThreadPoolManager::getInstance().schedule({this}, [cannon] { cannon->getController().delete_(); }, 3000);
		sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5b_TD_AddWave_01(), 3000);
		spawnWithDelay(231160, 707.703f, 259.173f, 253.038f, int8_t{40}, 33000); // Assault Pod
	}
}

// Java EternalBastionInstance.java:778-792
void EternalBastionInstance::onEndEffect(Effect& effect) {
	runtime::Ptr<Player> player = runtime::as<Player>(effect.getEffected());
	if (player != nullptr && !player->isDead() && !player->getLifeStats()->isAboutToDie()) {
		switch (effect.getSkillId()) {
			case 21138:
			case 21139: { // Cannons respawn if not killed
				const float x = player->getX();
				const float y = player->getY();
				const float z = player->getZ(); // Java: new Point3D(player.getX(), player.getY(), player.getZ())
				Race race = player->getRace();
				spawnTasks.add(ThreadPoolManager::getInstance().schedule({this}, [this, x, y, z, race] {
					if (instanceReward.get()->getInstanceProgressionType() == InstanceProgressionType::START_PROGRESS)
						spawn(race == Race::ELYOS ? 701596 : 701610, x, y, z, int8_t{50});
				}, 10 * 1000));
				break;
			}
		}
	}
}

// Java EternalBastionInstance.java:794-797
void EternalBastionInstance::onInstanceDestroy() {
	cancelTasks();
}

// Java EternalBastionInstance.java:799-805
bool EternalBastionInstance::onReviveEvent(Player& player) {
	PlayerReviveService::revive(player, 100, 100, false, 0);
	player.getGameStats()->updateStatsAndSpeedVisually();
	TeleportService::teleportTo(player, *instance, 449.581f, 448.846f, 270.747f, int8_t{70});
	return true;
}

// Java EternalBastionInstance.java:807-816
void EternalBastionInstance::cancelTasks() {
	for (const runtime::FutureRef& task : spawnTasks) {
		if (task != nullptr && !task->isDone())
			task->cancel(false);
	}
	if (runtime::FutureRef task = instanceTimerTask.get(); task != nullptr && !task->isCancelled())
		task->cancel(false);
	if (runtime::FutureRef task = assaultWaveTask.get(); task != nullptr && !task->isCancelled())
		task->cancel(false);
}

// Java EternalBastionInstance.java:818-820
void EternalBastionInstance::spawnWithDelay(int32_t npcId, float x, float y, float z, int8_t h, int32_t delay) {
	spawnTasks.add(ThreadPoolManager::getInstance().schedule({this}, [this, npcId, x, y, z, h] { spawn(npcId, x, y, z, h); }, delay));
}

// Java EternalBastionInstance.java:822-824 (a failed spawn: Java's NullPointerException)
void EternalBastionInstance::spawnWithWalker(int32_t npcId, float x, float y, float z, int8_t h, std::string_view walker) {
	spawn(npcId, x, y, z, h)->getSpawn()->setWalkerId(walker);
}

// Java EternalBastionInstance.java:826-832
void EternalBastionInstance::addPoints(Npc& npc, int32_t points) {
	if (instanceReward.get()->getInstanceProgressionType() == InstanceProgressionType::START_PROGRESS) {
		instanceReward.get()->addPoints(points);
		PacketSendUtility::broadcastToMap(*instance, SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE(npc.getObjectTemplate()->getL10n(), points));
		sendPacket();
	}
}

// Java EternalBastionInstance.java:834-837
int32_t EternalBastionInstance::getTime() {
	int32_t current = static_cast<int32_t>(commons::utils::currentTimeMillis() - startTime.get());
	return instanceReward.get()->getInstanceProgressionType() == InstanceProgressionType::PREPARING ? 180000 - current
	                                                                                                 : std::max(1800000 - current, 0);
}

// Java EternalBastionInstance.java:839-841
void EternalBastionInstance::sendPacket() {
	runtime::Ref<network::aion::instanceinfo::EternalBastionScoreWriter> writer =
		network::aion::instanceinfo::EternalBastionScoreWriter::create(*instanceReward.get());
	PacketSendUtility::broadcastToMap(*instance, SM_INSTANCE_SCORE(instance->getMapId(), *writer, getTime()));
}

// Java EternalBastionInstance.java:843-847
void EternalBastionInstance::leaveInstance(Player& player) {
	if (instanceReward.get()->getInstanceProgressionType() == InstanceProgressionType::END_PROGRESS)
		TeleportService::moveToInstanceExit(player, mapId, player.getRace());
}

// Java EternalBastionInstance.java:849-858
std::string EternalBastionInstance::getRankNameById(int32_t rank) {
	switch (rank) {
		case 1:
			return "S";
		case 2:
			return "A";
		case 3:
			return "B";
		case 4:
			return "C";
		case 5:
			return "D";
		default:
			return "F";
	}
}

// Java EternalBastionInstance.java:860-866
bool EternalBastionInstance::isBoss(Npc& npc) {
	switch (npc.getNpcId()) {
		case 209516:
		case 209517:
		case 231168:
		case 231169:
		case 231170:
		case 231171:
		case 231172:
		case 231173:
		case 231174:
		case 231175:
		case 231176:
		case 231130:
			return true;
		default:
			return false;
	}
}

} // namespace aion::gameserver::handlers::instance
