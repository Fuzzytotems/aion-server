#include "aion/gameserver/handlers/instance/SeizedDanuarSanctuaryInstance.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(SeizedDanuarSanctuaryInstance, 301140000);

// Java SeizedDanuarSanctuaryInstance.java:30-32
SeizedDanuarSanctuaryInstance::SeizedDanuarSanctuaryInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<SeizedDanuarSanctuaryInstance> SeizedDanuarSanctuaryInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<SeizedDanuarSanctuaryInstance>(instance);
}

// Java SeizedDanuarSanctuaryInstance.java:34-37
void SeizedDanuarSanctuaryInstance::onInstanceCreate() {
	spawnRndBoss();
}

// Java SeizedDanuarSanctuaryInstance.java:39-45
void SeizedDanuarSanctuaryInstance::onEnterInstance(Player& player) {
	if (!instanceRace.get().has_value() && started.compareAndSet(false, true)) {
		instanceRace = std::optional<Race>(player.getRace());
		spawnQuestNpcs();
	}
}

// Java SeizedDanuarSanctuaryInstance.java:47-67
void SeizedDanuarSanctuaryInstance::onDie(Npc& npc) {
	GeneralInstanceHandler::onDie(npc);
	switch (npc.getNpcId()) {
		case 233187:
			spawn(233087, 906.4945f, 861.5854f, 280.5441f, int8_t{73}, 1699);
			npc.getController().delete_();
			break;
		case 235620:
		case 235619:
		case 235621:
			spawn(701876, 1071.9772f, 682.4911f, 282.0391f, int8_t{60}); // Emergency Exit
			break;
		case 730866:
		case 233448:
		case 233447:
			npc.getController().delete_();
			break;
	}
}

// Java SeizedDanuarSanctuaryInstance.java:69-95
void SeizedDanuarSanctuaryInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	switch (npc.getNpcId()) {
		case 701873:
			TeleportService::teleportTo(*player, *instance, 1029.273f, 362.651f, 297.89f, int8_t{30}, TeleportAnimation::FADE_OUT_BEAM);
			break;
		case 701871:
			TeleportService::teleportTo(*player, *instance, 1006.0412f, 1366.468f, 337.26f, int8_t{105}, TeleportAnimation::FADE_OUT_BEAM);
			break;
		case 701872:
			TeleportService::teleportTo(*player, *instance, 846.172f, 991.731f, 300.04f, int8_t{110}, TeleportAnimation::FADE_OUT_BEAM);
			break;
		case 730863:
			if (cannonUsed.compareAndSet(false, true)) {
				runtime::Ptr<Npc> target = instance->getNpc(730866);
				SkillEngine::getInstance().getSkill(npc, 20385, 1, runtime::Ptr<VisibleObject>(target))->useWithoutPropSkill();
				if (target != nullptr) {
					ThreadPoolManager::getInstance().schedule({this}, [target = runtime::Ref<Npc>(target)] { target->getController().die(); }, 5700);
				}
			}
			break;
		case 701876:
			TeleportService::moveToInstanceExit(*player, mapId, player->getRace());
			break;
	}
}

// Java SeizedDanuarSanctuaryInstance.java:97-104
bool SeizedDanuarSanctuaryInstance::onReviveEvent(Player& player) {
	PlayerReviveService::revive(player, 25, 25, true, 0);
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	player.getGameStats()->updateStatsAndSpeedVisually();
	TeleportService::teleportTo(player, *instance, 388.6437f, 1184.639f, 55.30134f);
	return true;
}

// Java SeizedDanuarSanctuaryInstance.java:106-113
void SeizedDanuarSanctuaryInstance::spawnRndBoss() {
	/*
	 * 235619 Warmage Suyaroka
	 * 235620 Chief Medic Tagnu
	 * 235621 Virulent Ukahim
	 */
	spawn((235619 + commons::utils::Rnd::get(0, 2)), 1056.6105f, 694.0836f, 282.04f, int8_t{30});
}

// Java SeizedDanuarSanctuaryInstance.java:115-135
void SeizedDanuarSanctuaryInstance::spawnQuestNpcs() {
	if (instanceRace.get() == Race::ELYOS) {
		spawn(233129, 1022.065f, 465.547f, 290.2684f, int8_t{10});
		spawn(233129, 867.786f, 911.695f, 293.007f, int8_t{80});
		spawn(233129, 1068.531f, 481.1384f, 285.4618f, int8_t{110});
		spawn(233129, 920.645f, 923.6408f, 293.1628f, int8_t{110});
		spawn(233130, 1034.348f, 482.7226f, 290.3569f, int8_t{40});
		spawn(233130, 972.4765f, 1327.148f, 335.876f, int8_t{100});
		spawn(233131, 1103.7655f, 440.8195f, 284.6165f, int8_t{70});
		spawn(233131, 954.9696f, 910.779f, 299.523f, int8_t{110});
	} else {
		spawn(233126, 1022.065f, 465.547f, 290.2684f, int8_t{10});
		spawn(233126, 1068.531f, 481.1384f, 285.4618f, int8_t{110});
		spawn(233126, 867.786f, 911.695f, 293.007f, int8_t{80});
		spawn(233126, 920.645f, 923.6408f, 293.1628f, int8_t{110});
		spawn(233127, 1034.348f, 482.7226f, 290.3569f, int8_t{40});
		spawn(233127, 972.4765f, 1327.148f, 335.876f, int8_t{100});
		spawn(233128, 1103.7655f, 440.8195f, 284.6165f, int8_t{70});
		spawn(233128, 954.9696f, 910.779f, 299.523f, int8_t{110});
	}
}

} // namespace aion::gameserver::handlers::instance
