#include "aion/gameserver/handlers/instance/pvp/IronWallWarfrontInstance.h"

#include <cstdlib>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/instance/instancescore/PvpInstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/PvpInstancePlayerReward.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceGroup.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance::pvp {

AION_INSTANCE_HANDLER(IronWallWarfrontInstance, 301220000);

// Java IronWallWarfrontInstance.java:23-25
IronWallWarfrontInstance::IronWallWarfrontInstance(WorldMapInstance& instance) : BasicPvpInstance(instance) {
}

runtime::Ref<IronWallWarfrontInstance> IronWallWarfrontInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<IronWallWarfrontInstance>(instance);
}

// Java IronWallWarfrontInstance.java:27-32
void IronWallWarfrontInstance::onStart() {
	updateProgress(InstanceProgressionType::PREPARING);
	for (const runtime::Ptr<Player>& player : instance->getPlayersInside())
		portToStartPosition(*player); // split groups
	tasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] { endPreparingAndStart(); }, 60000));
}

// Java IronWallWarfrontInstance.java:34-38
void IronWallWarfrontInstance::endPreparingAndStart() {
	updateProgress(InstanceProgressionType::START_PROGRESS);
	openFirstDoors();
	tasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] { onStop(false); }, 2400000));
}

// Java IronWallWarfrontInstance.java:40-61
void IronWallWarfrontInstance::setAndDistributeRewards(Player& player, PvpInstancePlayerReward& reward, Race winningRace, bool isBossKilled) {
	int32_t scorePoints = instanceScore.get()->getPointsByRace(reward.getRace());
	if (reward.getRace() == winningRace) {
		reward.setBaseAp(instanceScore.get()->getWinnerApReward() + (isBossKilled ? 3850 : 0)); // increased by 3850 if pashid is killed
		reward.setBonusAp(2 * scorePoints / MAX_PLAYERS_PER_FACTION);
		reward.setBaseGp(100);
		reward.setReward1(186000243, 9, 0); // Fragmented Ceramium
		if (isBossKilled) {
			reward.setReward2(188052729, 1, 0); // Eternal Bastion Warfront Reward Chest
			if (commons::utils::Rnd::chance() < 5)
				reward.setReward3(188950020, 1); // CUSTOM: Special Courier Pass (Abyss Mythic/Lv. 61-65)
		}
	} else {
		reward.setBaseAp(instanceScore.get()->getLoserApReward());
		reward.setBonusAp(scorePoints / MAX_PLAYERS_PER_FACTION);
		reward.setBaseGp(10);
		if (winningRace == Race::NONE)
			reward.setBaseAp(instanceScore.get()->getDrawApReward()); // Base AP are overridden in a draw case
	}
	distributeRewards(player, reward);
}

// Java IronWallWarfrontInstance.java:63-70
void IronWallWarfrontInstance::updatePoints(runtime::Ptr<Player> player, Race race, std::optional<std::string_view> npcL10n, int32_t points) {
	BasicPvpInstance::updatePoints(player, race, npcL10n, points);

	int32_t diff = std::abs(instanceScore.get()->getAsmodiansPoints() - instanceScore.get()->getElyosPoints());
	if (diff >= 30000)
		onStop(false);
}

// Java IronWallWarfrontInstance.java:72-99
void IronWallWarfrontInstance::onDie(Npc& npc) {
	runtime::Ptr<Player> player = npc.getAggroList().getMostPlayerDamage();
	if (player == nullptr) {
		return;
	}
	int32_t points = 0;
	switch (npc.getNpcId()) {
		case 233473:
			points = 100;
			break;
		case 702042:
			points = 500;
			break;
		case 233491: // random position boss
		case 701943: // elyos power generator
		case 701944: // asmodian power generator
		case 701945: // balaur power generator
			points = 5000;
			break;
		case 233494:
			points = 30000;
			break;
	}
	if (points > 0) {
		updatePoints(player, player->getRace(), npc.getObjectTemplate()->getL10n(), points);
	}
}

// Java IronWallWarfrontInstance.java:101-139
void IronWallWarfrontInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	if (player == nullptr) {
		return;
	}
	int32_t points = 0;
	switch (npc.getNpcId()) {
		case 801903:
			points = 1500;
			break;
		case 801772:
			points = 525;
			break;
		case 801766:
		case 801767:
		case 801818:
		case 801819:
		case 801820:
		case 801821:
			points = 255;
			break;
		case 730861:
		case 730878:
		case 730879:
		case 730880:
			updatePoints(player, player->getRace(), npc.getObjectTemplate()->getL10n(), 200);
			if (player->getRace() == Race::ELYOS) {
				spawn(701900, npc.getX(), npc.getY(), npc.getZ(), npc.getHeading());
			} else {
				spawn(701901, npc.getX(), npc.getY(), npc.getZ(), npc.getHeading());
				npc.getController().delete_();
			}
			break;
	}
	if (points > 0) {
		updatePoints(player, player->getRace(), npc.getObjectTemplate()->getL10n(), points);
		npc.getController().delete_();
	}
}

// Java IronWallWarfrontInstance.java:141-144
void IronWallWarfrontInstance::openFirstDoors() {
	instance->setDoorState(177, true);
	instance->setDoorState(176, true);
}

// Java IronWallWarfrontInstance.java:146-149
int32_t IronWallWarfrontInstance::getReinforceMemberPhaseDelay() {
	return 120000;
}

// Java IronWallWarfrontInstance.java:151-155
void IronWallWarfrontInstance::onInstanceCreate() {
	instanceScore = PvpInstanceScore::create(8400, 1680, 5040); // No info found for draws, so let's guess
	BasicPvpInstance::onInstanceCreate();
}

// Java IronWallWarfrontInstance.java:157-171. Correction (owner's decision 2026-10-15, both branches): Java's portToPosition(Player,
// WorldMapInstance) overrides nothing; this is portToStartPosition, and the teleports go to the handler's instance.
void IronWallWarfrontInstance::portToStartPosition(Player& player) {
	bool useAlternativePos = player.isInAlliance()
		&& (player.getPlayerAllianceGroup()->getObjectId() == 1001 || player.getPlayerAllianceGroup()->getObjectId() == 1003);
	if ((player.getRace() == Race::ELYOS && raceStartPosition == 0) || (player.getRace() == Race::ASMODIANS && raceStartPosition != 0)) {
		if (useAlternativePos)
			TeleportService::teleportTo(player, *instance, 274.143f, 384.335f, 239.973f, int8_t{14});
		else
			TeleportService::teleportTo(player, *instance, 342.138f, 616.856f, 248.197f, int8_t{35});
	} else {
		if (useAlternativePos)
			TeleportService::teleportTo(player, *instance, 598.229f, 712.984f, 223.306f, int8_t{73});
		else
			TeleportService::teleportTo(player, *instance, 711.403f, 621.797f, 213.276f, int8_t{31});
	}
}

} // namespace aion::gameserver::handlers::instance::pvp
