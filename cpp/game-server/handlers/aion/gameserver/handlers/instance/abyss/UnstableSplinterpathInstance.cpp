#include "aion/gameserver/handlers/instance/abyss/UnstableSplinterpathInstance.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance::abyss {

AION_INSTANCE_HANDLER(UnstableSplinterpathInstance, 300600000);

// Java UnstableSplinterpathInstance.java:25-27
UnstableSplinterpathInstance::UnstableSplinterpathInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<UnstableSplinterpathInstance> UnstableSplinterpathInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<UnstableSplinterpathInstance>(instance);
}

// Java UnstableSplinterpathInstance.java:30-37
void UnstableSplinterpathInstance::onSpawn(VisibleObject& object) {
	if (runtime::Ptr<Npc> npc = runtime::as<Npc>(runtime::Ptr<VisibleObject>(object))) {
		switch (npc->getNpcId()) {
			case 219563:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdDH_Wakeup());
				break;
			case 219555:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdD_Wakeup());
				break;
			default:
				break;
		}
	}
}

// Java UnstableSplinterpathInstance.java:40-116 (Java's returning boss is the other one of the pair, and the pieces delete the Abyssal
// Splinter's ids 281907/281908: ported as written, proposed corrections J-P6I4-2 and J-P6I4-3)
void UnstableSplinterpathInstance::onDie(Npc& npc) {
	GeneralInstanceHandler::onDie(npc);
	const int32_t npcId = npc.getNpcId();
	switch (npcId) {
		case 219554: // Unstable Pazuzu
			spawnPazuzuFragment();
			spawnPazuzuTreasureBoxes();
			break;
		case 219553: // Unstable Kaluva the Fourth
			spawnKaluvaFragment();
			spawnKaluvaTreasureBoxes();
			break;
		case 219551: // Unstable Rukril
		case 219552: // Unstable Ebonsoul
			if (getNpc(npcId == 219552 ? 219551 : 219552) == nullptr) {
				spawnDayshadeFragment();
				spawnDayshadeTreasureBoxes();
			} else {
				sendMsg(npcId == 219551 ? SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdC_Light_Die() : SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdC_Dark_Die());
				ThreadPoolManager::getInstance().schedule({this}, [this, npcId] {
					if (getNpc(npcId == 219552 ? 219551 : 219552) != nullptr) {
						switch (npcId) {
							case 219551:
								spawn(219552, 447.1937f, 683.72217f, 433.1805f, int8_t{108}); // rukril
								break;
							case 219552:
								spawn(219551, 455.5502f, 702.09485f, 433.13727f, int8_t{108}); // ebonsoul
								break;
							default:
								break;
						}
					}
				}, 60000);
			}
			npc.getController().delete_();
			break;
		case 283204: { // Piece of Splendor
			runtime::Ptr<Npc> ebonsoul = getNpc(219552);
			if (ebonsoul != nullptr && !ebonsoul->isDead() && PositionUtil::isInRange(npc, *ebonsoul, 5)) {
				ebonsoul->getEffectController()->removeEffect(19159);
				deleteAliveNpcs({281907});
				break;
			}
			npc.getController().delete_();
			break;
		}
		case 283205: { // Piece of Midnight
			runtime::Ptr<Npc> rukril = getNpc(219551);
			if (rukril != nullptr && !rukril->isDead() && PositionUtil::isInRange(npc, *rukril, 5)) {
				rukril->getEffectController()->removeEffect(19266);
				deleteAliveNpcs({281908});
				break;
			}
			npc.getController().delete_();
			break;
		}
		case 219555: // Durable Yamennes Blindsight
		case 219563: // Unstable Yamennes Painflare
			spawnYamennesTreasureBoxes(npcId == 219555 ? 701579 : 701580);
			deleteAliveNpcs({219586}); // Summoned Unstable Ametgolem
			spawn(730317, 328.476f, 762.585f, 197.479f, int8_t{90}); // Exit
			instance->forEachPlayer([](Player& p) { SkillEngine::getInstance().applyEffectDirectly(19283, p, p); });
			break;
		case 701588: // Huge Aether Fragment
			destroyedFragments = destroyedFragments + 1;
			onFragmentKill();
			npc.getController().delete_();
			break;
		case 283206: // Luminous Waterworm
			killedPazuzuWorms = killedPazuzuWorms + 1;
			if (killedPazuzuWorms.get() == 4) {
				killedPazuzuWorms = 0;
				runtime::Ptr<Npc> pazuzu = getNpc(219554);
				if (pazuzu != nullptr && !pazuzu->isDead()) {
					pazuzu->getEffectController()->removeEffect(19145);
					pazuzu->getEffectController()->removeEffect(19291);
				}
			}
			break;
		case 219567: // Spawn Gate
		case 219579: // Spawn Gate
		case 219580: // Spawn Gate
			deleteSummons();
			break;
		default:
			break;
	}
}

// Java UnstableSplinterpathInstance.java:119-121
void UnstableSplinterpathInstance::onInstanceDestroy() {
	destroyedFragments = 0;
}

// Java UnstableSplinterpathInstance.java:124-135
void UnstableSplinterpathInstance::handleUseItemFinish(runtime::Ptr<Player> /*player*/, Npc& npc) {
	int32_t bossId; // Artifact of Protection
	switch (npc.getNpcId()) {
		case 700957:
			bossId = 219563; // Unstable Yamennes Painflare (Hard Mode)
			break;
		case 701589:
			bossId = 219555; // Durable Yamennes Blindsight (Easy Mode)
			break;
		default:
			bossId = 0;
			break;
	}
	if (bossId != 0) {
		sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdDH_Wakeup());
		spawn(bossId, 329.70886f, 733.8744f, 197.60938f, int8_t{0});
		npc.getController().delete_();
	}
}

// Java UnstableSplinterpathInstance.java:137-139
void UnstableSplinterpathInstance::spawnPazuzuFragment() {
	spawn(701588, 669.576f, 335.135f, 465.895f, int8_t{0});
}

// Java UnstableSplinterpathInstance.java:141-147
void UnstableSplinterpathInstance::spawnPazuzuTreasureBoxes() {
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdC_BoxSpawn());
	spawn(701575, 649.24286f, 361.33755f, 466.0427f, int8_t{33}); // Abyssal Treasure Box
	spawn(701576, 651.53204f, 357.085f, 466.1315f, int8_t{66}); // Genesis Treasure Box
	spawn(701576, 647.00446f, 357.2484f, 465.8960f, int8_t{0}); // Genesis Treasure Box
	spawn(701576, 653.8384f, 360.39508f, 466.4391f, int8_t{100}); // Genesis Treasure Box
}

// Java UnstableSplinterpathInstance.java:149-151
void UnstableSplinterpathInstance::spawnKaluvaFragment() {
	spawn(701588, 633.7498f, 557.8822f, 424.99347f, int8_t{6});
}

// Java UnstableSplinterpathInstance.java:153-159
void UnstableSplinterpathInstance::spawnKaluvaTreasureBoxes() {
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdC_BoxSpawn());
	spawn(701576, 601.2931f, 584.66705f, 422.9955f, int8_t{6}); // Genesis Treasure Box
	spawn(701576, 597.2156f, 583.95416f, 423.3474f, int8_t{66}); // Genesis Treasure Box
	spawn(701576, 602.9586f, 589.2678f, 422.8296f, int8_t{100}); // Genesis Treasure Box
	spawn(701577, 598.82776f, 588.25946f, 422.7739f, int8_t{113}); // Abyssal Treasure Box
}

// Java UnstableSplinterpathInstance.java:161-163
void UnstableSplinterpathInstance::spawnDayshadeFragment() {
	spawn(701588, 452.89706f, 692.36084f, 433.96838f, int8_t{6});
}

// Java UnstableSplinterpathInstance.java:165-171 ((byte) 130 is -126)
void UnstableSplinterpathInstance::spawnDayshadeTreasureBoxes() {
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdC_BoxSpawn());
	spawn(701576, 408.10938f, 650.9015f, 439.28332f, int8_t{66}); // Genesis Treasure Box
	spawn(701576, 402.40375f, 655.55237f, 439.26288f, int8_t{33}); // Genesis Treasure Box
	spawn(701576, 406.74445f, 655.5914f, 439.2548f, int8_t{100}); // Genesis Treasure Box
	spawn(701578, 404.891f, 650.2943f, 439.2548f, static_cast<int8_t>(130)); // Abyssal Treasure Box
}

// Java UnstableSplinterpathInstance.java:173-179
void UnstableSplinterpathInstance::spawnYamennesTreasureBoxes(int32_t npcId) {
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdC_BoxSpawn());
	spawn(701576, 326.978f, 729.8414f, 197.7078f, int8_t{16}); // Genesis Treasure Box
	spawn(701576, 326.5296f, 735.13324f, 197.6681f, int8_t{66}); // Genesis Treasure Box
	spawn(701576, 329.8462f, 738.41095f, 197.7329f, int8_t{3}); // Genesis Treasure Box
	spawn(npcId, 330.891f, 733.2943f, 197.6404f, int8_t{113}); // Abyssal Treasure Box
}

// Java UnstableSplinterpathInstance.java:181-184
void UnstableSplinterpathInstance::deleteSummons() {
	bool allDead = true;
	for (const runtime::Ptr<Npc>& gate : instance->getNpcs({219567, 219579, 219580}))
		allDead = allDead && gate->isDead();
	if (allDead)
		deleteAliveNpcs({219565, 219566}); // Summoned Unstable Orkanimum, Summoned Unstable Lapilima
}

// Java UnstableSplinterpathInstance.java:186-196
void UnstableSplinterpathInstance::onFragmentKill() {
	switch (destroyedFragments.get()) {
		case 1:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_Artifact_Die_01());
			break;
		case 2:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_Artifact_Die_02());
			break;
		case 3:
			deleteAliveNpcs({701589}); // Artifact of Protection (Easy Mode)
			spawn(700957, 326.1821f, 766.9640f, 202.1832f, int8_t{100}, 79); // Artifact of Protection (Hard Mode)
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_Artifact_Die_03());
			break;
		default:
			break;
	}
}

} // namespace aion::gameserver::handlers::instance::abyss
