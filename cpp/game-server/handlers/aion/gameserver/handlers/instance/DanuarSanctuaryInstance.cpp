#include "aion/gameserver/handlers/instance/DanuarSanctuaryInstance.h"

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

AION_INSTANCE_HANDLER(DanuarSanctuaryInstance, 301380000);

// Java DanuarSanctuaryInstance.java:27-29
DanuarSanctuaryInstance::DanuarSanctuaryInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<DanuarSanctuaryInstance> DanuarSanctuaryInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<DanuarSanctuaryInstance>(instance);
}

// Java DanuarSanctuaryInstance.java:31-34
void DanuarSanctuaryInstance::onInstanceCreate() {
	spawnRndBoss();
}

// Java DanuarSanctuaryInstance.java:36-56
void DanuarSanctuaryInstance::onDie(Npc& npc) {
	GeneralInstanceHandler::onDie(npc);
	switch (npc.getNpcId()) {
		case 233187:
			spawn(233087, 906.4945f, 861.5854f, 280.5441f, int8_t{73}, 1699);
			npc.getController().delete_();
			break;
		case 235624:
		case 235625:
		case 235626:
			spawn(701876, 1071.9772f, 682.4911f, 282.0391f, int8_t{60}); // Emergency Exit
			break;
		case 730866:
		case 233448:
		case 233447:
			npc.getController().delete_();
			break;
	}
}

// Java DanuarSanctuaryInstance.java:58-84
void DanuarSanctuaryInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
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

// Java DanuarSanctuaryInstance.java:86-93
bool DanuarSanctuaryInstance::onReviveEvent(Player& player) {
	PlayerReviveService::revive(player, 25, 25, true, 0);
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	player.getGameStats()->updateStatsAndSpeedVisually();
	TeleportService::teleportTo(player, *instance, 388.6437f, 1184.639f, 55.30134f);
	return true;
}

// Java DanuarSanctuaryInstance.java:95-102
void DanuarSanctuaryInstance::spawnRndBoss() {
	/*
	 * 235624 Warmage Suyaroka
	 * 235625 Chief Medic Tagnu
	 * 235626 Virulent Ukahim
	 */
	spawn((235624 + commons::utils::Rnd::get(0, 2)), 1056.6105f, 694.0836f, 282.04f, int8_t{30});
}

} // namespace aion::gameserver::handlers::instance
