#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionTankControllerAI.h"

#include <optional>
#include <vector>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/DamageInfo.h"
#include "aion/gameserver/controllers/attack/DamageList.h"
#include "aion/gameserver/model/templates/npc/NpcTemplateType.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"
#include "aion/gameserver/services/panesterra/ahserion/AhserionRaid.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraTeam.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

AION_AI(AhserionTankControllerAI, "ahserion_tank_controller");

using services::panesterra::ahserion::PanesterraFaction;

// Java AhserionTankControllerAI.java:30-34
void AhserionTankControllerAI::handleAttack(runtime::Ptr<Creature> creature) {
	AhserionConstructAI::handleAttack(creature);
	broadcastAttack();
}

// Java AhserionTankControllerAI.java:36-56
void AhserionTankControllerAI::broadcastAttack() {
	if (canShout.compareAndSet(true, false)) {
		if (!getOwner().isDead() && getOwner().getWorldId() == 400030000) {
			switch (getOwner().getSpawn()->getStaticId()) {
				case 180:
					PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_TANK_C_ATTACKED());
					break;
				case 181:
					PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_TANK_B_ATTACKED());
					break;
				case 182:
					PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_TANK_D_ATTACKED());
					break;
				case 183:
					PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_TANK_A_ATTACKED());
					break;
				default:
					break;
			}
			ThreadPoolManager::getInstance().schedule({this}, [this] { allowShout(); }, 3000);
		}
	}
}

// Java AhserionTankControllerAI.java:58-61
void AhserionTankControllerAI::allowShout() {
	if (!isDead())
		canShout.set(true);
}

// Java AhserionTankControllerAI.java:63-71
void AhserionTankControllerAI::deleteRelatedNpcs() {
	getKnownList().forEachNpc([this](Npc& npc) {
		if (PositionUtil::isInRange(getOwner(), npc, 40)) {
			// Tanks, Flag, Attacker
			if (npc.getRace() == Race::CONSTRUCT || npc.getNpcTemplateType() == model::templates::npc::NpcTemplateType::FLAG || npc.getNpcId() == 297185)
				npc.getController().deleteIfAliveOrCancelRespawn();
		}
	});
}

// Java AhserionTankControllerAI.java:73-77
void AhserionTankControllerAI::handleDespawned() {
	deleteRelatedNpcs();
	AhserionConstructAI::handleDespawned();
}

// Java AhserionTankControllerAI.java:79-115 (Java's HashMap keeps the null key of a player without a team; findWinnerTeam never reads it, so
// it is left out here)
void AhserionTankControllerAI::handleDied() {
	deleteRelatedNpcs();
	switch (getOwner().getSpawn()->getStaticId()) {
		case 180:
			PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_TANK_C_BROKEN());
			break;
		case 181:
			PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_TANK_B_BROKEN());
			break;
		case 182:
			PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_TANK_D_BROKEN());
			break;
		case 183:
			PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_TANK_A_BROKEN());
			break;
		default:
			break;
	}
	std::map<PanesterraFaction, int32_t> panesterraDamage;
	// Only players or balaur can attack the controller, there are no other npcs on this map
	for (const controllers::attack::DamageInfo& damageInfo : getAggroList().getFinalDamageList().getCreatureDamages()) {
		std::optional<PanesterraFaction> faction;
		if (runtime::Ptr<Player> player = runtime::as<Player>(damageInfo.getAttacker())) {
			runtime::Ptr<services::panesterra::ahserion::PanesterraTeam> team = PanesterraService::getInstance().getTeam(*player);
			if (team != nullptr && !team->isEliminated())
				faction = team->getFaction();
		} else {
			faction = PanesterraFaction::BALAUR;
		}
		if (faction)
			panesterraDamage[*faction] += damageInfo.getDamage(); // Java: merge(faction, damage, Integer::sum)
	}
	int32_t staticId = getSpawnTemplate()->getStaticId();
	// the winner is chosen when the task runs, 3 s later, as Java's lambda does
	ThreadPoolManager::getInstance().schedule({this}, [this, staticId, panesterraDamage] {
		spawnNewTankBase(staticId, findWinnerTeam(panesterraDamage));
	}, 3000);
	AhserionConstructAI::handleDied();
}

// Java AhserionTankControllerAI.java:117-133 (just in case: we'll spawn balaur construct again)
PanesterraFaction AhserionTankControllerAI::findWinnerTeam(const std::map<PanesterraFaction, int32_t>& panesterraDamage) {
	PanesterraFaction winner = PanesterraFaction::BALAUR;
	const auto balaur = panesterraDamage.find(PanesterraFaction::BALAUR);
	int32_t maxDmg = balaur == panesterraDamage.end() ? 0 : balaur->second;
	for (uint8_t value = 0; value <= static_cast<uint8_t>(PanesterraFaction::PEACE); value++) { // Java: PanesterraFaction.values()
		const PanesterraFaction faction = static_cast<PanesterraFaction>(value);
		const auto dmg = panesterraDamage.find(faction);
		runtime::Ptr<services::panesterra::ahserion::PanesterraTeam> team = PanesterraService::getInstance().getTeam(faction);
		if (dmg != panesterraDamage.end() && team != nullptr && !team->isEliminated()) {
			if (dmg->second > maxDmg) {
				maxDmg = dmg->second;
				winner = faction;
			}
		}
	}
	return winner;
}

// Java AhserionTankControllerAI.java:135-138
void AhserionTankControllerAI::spawnNewTankBase(int32_t staticId, PanesterraFaction faction) {
	if (AhserionRaid::getInstance().isStarted())
		AhserionRaid::getInstance().spawnStage(staticId, faction);
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
