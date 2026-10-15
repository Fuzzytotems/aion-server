#include "aion/gameserver/handlers/instance/abyss/AbyssalSplinterInstance.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance::abyss {

AION_INSTANCE_HANDLER(AbyssalSplinterInstance, 300220000);

// Java AbyssalSplinterInstance.java:27-29
AbyssalSplinterInstance::AbyssalSplinterInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<AbyssalSplinterInstance> AbyssalSplinterInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<AbyssalSplinterInstance>(instance);
}

// Java AbyssalSplinterInstance.java:32-39
void AbyssalSplinterInstance::onSpawn(VisibleObject& object) {
	if (runtime::Ptr<Npc> npc = runtime::as<Npc>(runtime::Ptr<VisibleObject>(object))) {
		switch (npc->getNpcId()) {
			case 216960:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdDH_Wakeup());
				break;
			case 216952:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdD_Wakeup());
				break;
			default:
				break;
		}
	}
}

// Java AbyssalSplinterInstance.java:42-122
void AbyssalSplinterInstance::onDie(Npc& npc) {
	GeneralInstanceHandler::onDie(npc);
	const int32_t npcId = npc.getNpcId();
	switch (npcId) {
		case 216951: // Pazuzu
			spawnPazuzuFragment();
			spawnPazuzuTreasureBoxes();
			break;
		case 216950: // Kaluva the Fourth Fragment
			spawnKaluvaFragment();
			spawnKaluvaTreasureBoxes();
			break;
		case 216948: // Rukril
		case 216949: // Ebonsoul
			if (getNpc(npcId == 216949 ? 216948 : 216949) == nullptr) {
				spawnDayshadeFragment();
				spawnDayshadeTreasureBoxes();
			} else {
				sendMsg(npcId == 216948 ? SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdC_Light_Die() : SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdC_Dark_Die());
				ThreadPoolManager::getInstance().schedule({this}, [this, npcId] {
					if (getNpc(npcId == 216949 ? 216948 : 216949) != nullptr) {
						switch (npcId) {
							case 216948:
								spawn(216948, 447.1937f, 683.72217f, 433.1805f, int8_t{108}); // rukril
								break;
							case 216949:
								spawn(216949, 455.5502f, 702.09485f, 433.13727f, int8_t{108}); // ebonsoul
								break;
							default:
								break;
						}
					}
				}, 60000);
			}
			npc.getController().delete_();
			break;
		case 281907: { // Piece of Splendor
			runtime::Ptr<Npc> ebonsoul = getNpc(216949);
			if (ebonsoul != nullptr && !ebonsoul->isDead()) {
				if (PositionUtil::isInRange(npc, *ebonsoul, 5)) {
					ebonsoul->getEffectController()->removeEffect(19159);
					deleteAliveNpcs({281907});
					break;
				}
			}
			npc.getController().delete_();
			break;
		}
		case 281908: { // Piece of Midnight
			runtime::Ptr<Npc> rukril = getNpc(216948);
			if (rukril != nullptr && !rukril->isDead()) {
				if (PositionUtil::isInRange(npc, *rukril, 5)) {
					rukril->getEffectController()->removeEffect(19266);
					deleteAliveNpcs({281908});
					break;
				}
			}
			npc.getController().delete_();
			break;
		}
		case 216960: // Yamennes Painflare
		case 216952: // Yamennes Blindsight
			spawnYamennesTreasureBoxes(npcId == 216952 ? 700937 : 700938);
			deleteAliveNpcs({282107});
			spawn(730317, 328.476f, 762.585f, 197.479f, int8_t{90}); // Exit
			break;
		case 700955: // Huge Aether Fragment
			destroyedFragments = destroyedFragments + 1;
			onFragmentKill();
			npc.getController().delete_();
			break;
		case 281909:
			killedPazuzuWorms = killedPazuzuWorms + 1;
			if (killedPazuzuWorms.get() == 5) {
				killedPazuzuWorms = 0;
				runtime::Ptr<Npc> pazuzu = getNpc(216951);
				if (pazuzu != nullptr && !pazuzu->isDead()) {
					pazuzu->getEffectController()->removeEffect(19145);
					pazuzu->getEffectController()->removeEffect(19291);
				}
			}
			npc.getController().delete_();
			break;
		case 282014: // Spawn Gate
		case 282015: // Spawn Gate
		case 282131: // Spawn Gate
			deleteSummons();
			break;
		default:
			break;
	}
}

// Java AbyssalSplinterInstance.java:125-127
void AbyssalSplinterInstance::onInstanceDestroy() {
	destroyedFragments = 0;
}

// Java AbyssalSplinterInstance.java:130-161
void AbyssalSplinterInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	switch (npc.getNpcId()) {
		case 700862: { // Broken Orkanimum
			int32_t itemId = player->getRace() == Race::ASMODIANS ? 182209820 : 182209800;
			if (player->getInventory().getFirstItemByItemId(itemId) == nullptr)
				ItemService::addItem(*player, itemId, 1);
			break;
		}
		case 700865: // Worn Book
			if (player->getRace() == Race::ASMODIANS && player->getInventory().getFirstItemByItemId(182209824) == nullptr)
				ItemService::addItem(*player, 182209824, 1);
			break;
		case 700864: // Polearm of Akarios
			if (player->getRace() == Race::ELYOS && player->getInventory().getFirstItemByItemId(182209803) == nullptr)
				ItemService::addItem(*player, 182209803, 1);
			break;
		case 701593: { // Artifact of Protection (Hard Mode)
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdDH_Wakeup());
			spawn(216960, 329.70886f, 733.8744f, 197.60938f, int8_t{0});
			int32_t artifactOfProtection = player->getRace() == Race::ELYOS ? 700857 : 700858; // for quest 30255 / 30355
			spawn(artifactOfProtection, 326.1821f, 766.9640f, 202.1832f, int8_t{100}, 79);
			npc.getController().die();
			break;
		}
		case 700856: { // Artifact of Protection (Easy Mode)
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdD_Wakeup());
			spawn(216952, 329.70886f, 733.8744f, 197.60938f, int8_t{0});
			int32_t artifactOfProtection = player->getRace() == Race::ELYOS ? 700857 : 700858; // for quest 30255 / 30355
			spawn(artifactOfProtection, 326.1821f, 766.9640f, 202.1832f, int8_t{100}, 79);
			npc.getController().die();
			break;
		}
		default:
			break;
	}
}

// Java AbyssalSplinterInstance.java:163-165
void AbyssalSplinterInstance::spawnPazuzuFragment() {
	spawn(700955, 669.576f, 335.135f, 465.895f, int8_t{0});
}

// Java AbyssalSplinterInstance.java:167-175
void AbyssalSplinterInstance::spawnPazuzuTreasureBoxes() {
	spawn(700934, 651.53204f, 357.085f, 466.1315f, int8_t{66}); // Genesis Treasure Box
	spawn(700934, 647.00446f, 357.2484f, 465.8960f, int8_t{0}); // Genesis Treasure Box
	spawn(700934, 653.8384f, 360.39508f, 466.4391f, int8_t{100}); // Genesis Treasure Box
	spawn(700860, 649.24286f, 361.33755f, 466.0427f, int8_t{33}); // Abyssal Treasure Box
	if (commons::utils::Rnd::chance() < 12)
		spawn(700861, 661.061f, 357.587f, 465.991f, int8_t{100}, 67); // Pazuzu's Treasure Box
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdC_BoxSpawn());
}

// Java AbyssalSplinterInstance.java:177-179
void AbyssalSplinterInstance::spawnKaluvaFragment() {
	spawn(700955, 633.7498f, 557.8822f, 424.99347f, int8_t{6});
}

// Java AbyssalSplinterInstance.java:181-187
void AbyssalSplinterInstance::spawnKaluvaTreasureBoxes() {
	spawn(700934, 601.2931f, 584.66705f, 422.9955f, int8_t{6}); // Genesis Treasure Box
	spawn(700934, 597.2156f, 583.95416f, 423.3474f, int8_t{66}); // Genesis Treasure Box
	spawn(700934, 602.9586f, 589.2678f, 422.8296f, int8_t{100}); // Genesis Treasure Box
	spawn(700935, 598.82776f, 588.25946f, 422.7739f, int8_t{113}); // Abyssal Treasure Box
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdC_BoxSpawn());
}

// Java AbyssalSplinterInstance.java:189-191
void AbyssalSplinterInstance::spawnDayshadeFragment() {
	spawn(700955, 452.89706f, 692.36084f, 433.96838f, int8_t{6});
}

// Java AbyssalSplinterInstance.java:193-199 ((byte) 130 is -126)
void AbyssalSplinterInstance::spawnDayshadeTreasureBoxes() {
	spawn(700934, 408.10938f, 650.9015f, 439.28332f, int8_t{66}); // Genesis Treasure Box
	spawn(700934, 402.40375f, 655.55237f, 439.26288f, int8_t{33}); // Genesis Treasure Box
	spawn(700934, 406.74445f, 655.5914f, 439.2548f, int8_t{100}); // Genesis Treasure Box
	spawn(700936, 404.891f, 650.2943f, 439.2548f, static_cast<int8_t>(130)); // Abyssal Treasure Box
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdC_BoxSpawn());
}

// Java AbyssalSplinterInstance.java:201-207
void AbyssalSplinterInstance::spawnYamennesTreasureBoxes(int32_t npcId) {
	spawn(700934, 326.978f, 729.8414f, 197.7078f, int8_t{16}); // Genesis Treasure Box
	spawn(700934, 326.5296f, 735.13324f, 197.6681f, int8_t{66}); // Genesis Treasure Box
	spawn(700934, 329.8462f, 738.41095f, 197.7329f, int8_t{3}); // Genesis Treasure Box
	spawn(npcId, 330.891f, 733.2943f, 197.6404f, int8_t{113}); // Abyssal Treasure Box
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdC_BoxSpawn());
}

// Java AbyssalSplinterInstance.java:209-212
void AbyssalSplinterInstance::deleteSummons() {
	bool allDead = true;
	for (const runtime::Ptr<Npc>& gate : instance->getNpcs({282014, 282015, 282131}))
		allDead = allDead && gate->isDead();
	if (allDead)
		deleteAliveNpcs({281903, 281904}); // Summoned Orkanimum, Summoned Lapilima
}

// Java AbyssalSplinterInstance.java:214-224
void AbyssalSplinterInstance::onFragmentKill() {
	switch (destroyedFragments.get()) {
		case 1:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_Artifact_Die_01());
			break;
		case 2:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_Artifact_Die_02());
			break;
		case 3:
			deleteAliveNpcs({700856}); // Artifact of Protection (Easy Mode)
			spawn(701593, 326.1821f, 766.9640f, 202.1832f, int8_t{100}, 79); // Artifact of Protection (Hard Mode)
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_Artifact_Die_03());
			break;
		default:
			break;
	}
}

} // namespace aion::gameserver::handlers::instance::abyss
