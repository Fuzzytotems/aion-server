#include "aion/gameserver/handlers/instance/dredgion/DredgionInstance.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/instance/DredgionRoom.h"
#include "aion/gameserver/model/instance/instancescore/PvpInstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/PvpInstancePlayerReward.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/instanceinfo/DredgionScoreWriter.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/stats/AbyssRankEnumInfo.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include <string>
#include <vector>


namespace aion::gameserver::handlers::instance::dredgion {

// Java DredgionInstance.java:56-58
DredgionInstance::DredgionInstance(WorldMapInstance& instance)
	: GeneralInstanceHandler(instance), isInstanceStarted(runtime::Rc<runtime::AtomicBoolean>::create(false)),
	  raceStartPosition(commons::utils::Rnd::nextInt(2)) {
}

runtime::Ref<DredgionInstance> DredgionInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<DredgionInstance>(instance);
}

// Java DredgionInstance.java:60-65
runtime::Ptr<PvpInstancePlayerReward> DredgionInstance::getPlayerReward(Player& player) {
	if (instanceScore.get()->getPlayerReward(player.getObjectId()) == nullptr) {
		addPlayerToReward(player);
	}
	return runtime::cast<PvpInstancePlayerReward>(instanceScore.get()->getPlayerReward(player.getObjectId()));
}

// Java DredgionInstance.java:67-73
void DredgionInstance::captureRoom(Race race, int32_t roomId) {
	for (runtime::Ptr<DredgionRoom> dredgionRoom : dredgionRooms) {
		if (dredgionRoom->getRoomId() == roomId) {
			dredgionRoom->captureRoom(race);
		}
	}
}

// Java DredgionInstance.java:75-77
void DredgionInstance::addPlayerToReward(Player& player) {
	instanceScore.get()->addPlayerReward(*PvpInstancePlayerReward::create(player.getObjectId(), player.getRace()));
}

// Java DredgionInstance.java:79-87
void DredgionInstance::startInstanceTask() {
	instanceTime = commons::utils::currentTimeMillis();
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		openFirstDoors();
		instanceScore.get()->setInstanceProgressionType(InstanceProgressionType::START_PROGRESS);
		sendPacket();
	}, 120000);
	instanceTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { stopInstance(instanceScore.get()->getRaceWithHighestPoints()); },
		2520000));
}

// Java DredgionInstance.java:90-94
void DredgionInstance::onEnterInstance(Player& player) {
	if (!instanceScore.get()->containsPlayer(player.getObjectId()))
		instanceScore.get()->addPlayerReward(*PvpInstancePlayerReward::create(player.getObjectId(), player.getRace()));
	sendPacket();
}

// Java DredgionInstance.java:97-99
void DredgionInstance::onInstanceCreate() {
	initializeInstance(4500, 2500, 3750);
}

// Java DredgionInstance.java:101-107
void DredgionInstance::initializeInstance(int32_t winnerAp, int32_t loserAp, int32_t drawAp) {
	instanceScore = PvpInstanceScore::create(winnerAp, loserAp, drawAp);
	instanceScore.get()->setInstanceProgressionType(InstanceProgressionType::PREPARING);
	for (int32_t i = 1; i < 15; i++) {
		dredgionRooms.add(DredgionRoom::create(i));
	}
}

// Java DredgionInstance.java:109-117
void DredgionInstance::stopInstance(Race winnerRace) {
	stopInstanceTask();
	instanceScore.get()->setInstanceProgressionType(InstanceProgressionType::END_PROGRESS);
	instance->forEachPlayer([this, winnerRace](Player& p) { doReward(p, *getPlayerReward(p), winnerRace); });
	instance->forEachNpc([](Npc& npc) { npc.getController().delete_(); });
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		for (const runtime::Ptr<Player>& player : instance->getPlayersInside())
			revivePlayerOnEnd(*player);
	}, 10000);
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		for (const runtime::Ptr<Player>& player : instance->getPlayersInside())
			leaveInstance(*player);
	}, 60000);
	sendPacket();
}

// Java DredgionInstance.java:119-131
void DredgionInstance::doReward(Player& player, PvpInstancePlayerReward& reward, Race winningRace) {
	int32_t scorePoints = instanceScore.get()->getPointsByRace(reward.getRace());
	if (reward.getRace() == winningRace) {
		reward.setBaseAp(instanceScore.get()->getWinnerApReward());
		reward.setBonusAp(2 * scorePoints / MAX_PLAYERS_PER_FACTION);
	} else {
		reward.setBaseAp(instanceScore.get()->getLoserApReward());
		reward.setBonusAp(scorePoints / MAX_PLAYERS_PER_FACTION);
		if (winningRace == Race::NONE)
			reward.setBaseAp(instanceScore.get()->getDrawApReward()); // Base AP are overridden in a draw case
	}
	distributeRewards(player, reward);
}

// Java DredgionInstance.java:133-147
void DredgionInstance::distributeRewards(Player& player, PvpInstancePlayerReward& reward) {
	runtime::Ref<QuestEnv> env = QuestEnv::create(nullptr, player, 0);
	QuestEngine::getInstance().onDredgionReward(*env);
	AbyssPointsService::addAp(player, model::gameobjects::player::calcResult(Rates::AP_DREDGION, player, reward.getBaseAp() + reward.getBonusAp()));
	if (reward.getReward1ItemId() > 0)
		ItemService::addItem(player, reward.getReward1ItemId(), reward.getReward1Count() + reward.getReward1BonusCount(), true);
	if (reward.getReward2ItemId() > 0)
		ItemService::addItem(player, reward.getReward2ItemId(), reward.getReward2Count() + reward.getReward2BonusCount(), true);
	if (reward.getReward3ItemId() > 0)
		ItemService::addItem(player, reward.getReward3ItemId(), reward.getReward3Count(), true);
	if (reward.getReward4ItemId() > 0)
		ItemService::addItem(player, reward.getReward4ItemId(), reward.getReward4Count(), true);
	if (reward.getBonusRewardItemId() > 0)
		ItemService::addItem(player, reward.getBonusRewardItemId(), reward.getBonusRewardCount(), true);
}

// Java DredgionInstance.java:150-156
bool DredgionInstance::onReviveEvent(Player& player) {
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	PlayerReviveService::revive(player, 100, 100, false, 0);
	player.getGameStats()->updateStatsAndSpeedVisually();
	portToStartPosition(player);
	return true;
}

// Java DredgionInstance.java:158-161
void DredgionInstance::revivePlayerOnEnd(Player& player) {
	if (player.isDead())
		PlayerReviveService::duelRevive(player);
}

// Java DredgionInstance.java:163-180 (`points *= f`: Java's compound assignment narrows the float product to int)
bool DredgionInstance::onDie(Player& player, Creature& lastAttacker) {
	int32_t points = 60;
	runtime::Ptr<Player> killer = runtime::as<Player>(runtime::Ptr<Creature>(lastAttacker));
	if (killer != nullptr && killer->getRace() != player.getRace()) {
		if (killer->getRace() != instanceScore.get()->getRaceWithHighestPoints())
			points = static_cast<int32_t>(static_cast<float>(points) * losingGroupMultiplier.get());
		else if (losingGroupMultiplier.get() == 10 || getPlayerReward(player)->getPoints() == 0)
			points = 0;
		if (utils::stats::id(player.getAbyssRank()->getRank()) - utils::stats::id(killer->getAbyssRank()->getRank()) >= 4)
			points = static_cast<int32_t>(static_cast<float>(points) * 1.6f);
		updateScore(*killer, runtime::Ptr<Creature>(player), points, true);
	}
	updateScore(player, runtime::Ptr<Creature>(player), -points, false);
	return true;
}

// Java DredgionInstance.java:182-184
void DredgionInstance::addPointToPlayer(Player& player, int32_t points) {
	getPlayerReward(player)->addPoints(points);
}

// Java DredgionInstance.java:186-188
void DredgionInstance::addPvPKillToPlayer(Player& player) {
	getPlayerReward(player)->addPvPKill();
}

// Java DredgionInstance.java:190-192
void DredgionInstance::addBalaurKillToPlayer(Player& player) {
	getPlayerReward(player)->addMonsterKillToPlayer();
}

// Java DredgionInstance.java:194-246
void DredgionInstance::updateScore(Player& player, runtime::Ptr<Creature> target, int32_t points, bool pvpKill) {
	if (points == 0)
		return;
	// group score
	instanceScore.get()->addPointsByRace(player.getRace(), points);
	// player score
	std::vector<runtime::Ptr<Player>> playersToGainScore;
	if (target != nullptr && player.isInGroup()) {
		for (const runtime::Ptr<Player>& member : player.getPlayerGroup()->getOnlineMembers()) {
			if (member->isDead()) {
				continue;
			}
			if (PositionUtil::isInRange(*member, *target, static_cast<float>(GroupConfig::GROUP_MAX_DISTANCE.load()))) {
				playersToGainScore.push_back(member);
			}
		}
	} else {
		playersToGainScore.push_back(runtime::Ptr<Player>(player));
	}
	for (const runtime::Ptr<Player>& playerToGainScore : playersToGainScore) {
		addPointToPlayer(*playerToGainScore, points / static_cast<int32_t>(playersToGainScore.size()));
		if (runtime::Ptr<Npc> npc = runtime::as<Npc>(target)) {
			PacketSendUtility::sendPacket(*playerToGainScore, SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE(npc->getObjectTemplate()->getL10n(), points));
		} else if (runtime::as<Player>(target) != nullptr) {
			PacketSendUtility::sendPacket(*playerToGainScore, SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE(target->getName(), points));
		}
	}
	// recalculate point multiplier
	int32_t pointDifference = instanceScore.get()->getAsmodiansPoints() - instanceScore.get()->getElyosPoints();
	if (pointDifference < 0) {
		pointDifference *= -1;
	}
	if (pointDifference >= 3000) {
		losingGroupMultiplier = 10;
	} else if (pointDifference >= 1000) {
		losingGroupMultiplier = 1.5f;
	} else {
		losingGroupMultiplier = 1;
	}
	// pvpKills for pvp and balaurKills for pve
	if (pvpKill && points > 0) {
		addPvPKillToPlayer(player);
	} else if (runtime::as<Npc>(target) != nullptr && target->getRace() == Race::DRAKAN) {
		addBalaurKillToPlayer(player);
	}
	sendPacket();
}

// Java DredgionInstance.java:249-261
void DredgionInstance::onDie(Npc& npc) {
	runtime::Ptr<Player> mostPlayerDamage = npc.getAggroList().getMostPlayerDamage();
	if (mostPlayerDamage == nullptr)
		return;
	int32_t hpGauge = npc.getObjectTemplate()->getHpGauge();
	if (hpGauge <= 5) {
		updateScore(*mostPlayerDamage, runtime::Ptr<Creature>(npc), 12, false);
	} else if (hpGauge <= 9) {
		updateScore(*mostPlayerDamage, runtime::Ptr<Creature>(npc), 32, false);
	} else {
		updateScore(*mostPlayerDamage, runtime::Ptr<Creature>(npc), 42, false);
	}
}

// Java DredgionInstance.java:264-268
void DredgionInstance::onInstanceDestroy() {
	stopInstanceTask();
	isInstanceDestroyed = true;
	instanceScore.get()->clear();
}

// Java DredgionInstance.java:270-271
void DredgionInstance::openFirstDoors() {
}

// Java DredgionInstance.java:273-276
void DredgionInstance::sendPacket() {
	std::vector<runtime::Ref<Player>> players;
	for (const runtime::Ptr<Player>& player : instance->getPlayersInside())
		players.emplace_back(*player);
	std::vector<runtime::Ref<DredgionRoom>> rooms;
	for (runtime::Ptr<DredgionRoom> room : dredgionRooms)
		rooms.emplace_back(*room);
	runtime::Ref<DredgionScoreWriter> writer = DredgionScoreWriter::create(*instanceScore.get(), std::move(players), std::move(rooms));
	PacketSendUtility::broadcastToMap(*instance, SM_INSTANCE_SCORE(instance->getMapId(), *writer, getTime()));
}

// Java DredgionInstance.java:278-286
int32_t DredgionInstance::getTime() {
	int64_t result = commons::utils::currentTimeMillis() - instanceTime.get();
	if (result < 120000) {
		return static_cast<int32_t>(120000 - result);
	} else if (result < 2520000) {
		return static_cast<int32_t>(2400000 - (result - 120000));
	}
	return 0;
}

// Java DredgionInstance.java:288-290
void DredgionInstance::sp(int32_t npcId, float x, float y, float z, int8_t h, int32_t time) {
	sp(npcId, x, y, z, h, 0, time);
}

// Java DredgionInstance.java:292-298
void DredgionInstance::sp(int32_t npcId, float x, float y, float z, int8_t h, int32_t staticId, int32_t time) {
	ThreadPoolManager::getInstance().schedule({this}, [this, npcId, x, y, z, h, staticId] {
		if (!isInstanceDestroyed.get()) {
			spawn(npcId, x, y, z, h, staticId);
		}
	}, time);
}

// Java DredgionInstance.java:300-308
void DredgionInstance::sp(int32_t npcId, float x, float y, float z, int8_t h, int32_t time, std::string_view walkerId) {
	ThreadPoolManager::getInstance().schedule({this}, [this, npcId, x, y, z, h, walker = std::string(walkerId)] {
		if (!isInstanceDestroyed.get()) {
			runtime::Ptr<Npc> npc = runtime::cast<Npc>(spawn(npcId, x, y, z, h));
			npc->getSpawn()->setWalkerId(walker);
			WalkManager::startWalking(dynamic_cast<NpcAI&>(npc->getAi()));
		}
	}, time);
}

// Java DredgionInstance.java:310-317
void DredgionInstance::sendMsgByRace(int32_t msg, Race race, int32_t time) {
	ThreadPoolManager::getInstance().schedule({this}, [this, msg, race] {
		instance->forEachPlayer([msg, race](Player& player) {
			if (player.getRace() == race || race == Race::PC_ALL) {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE(msg, std::vector<std::string>{}));
			}
		});
	}, time);
}

// Java DredgionInstance.java:319-323
void DredgionInstance::stopInstanceTask() {
	if (runtime::FutureRef task = instanceTask.get(); task != nullptr) {
		task->cancel(true);
	}
}

// Java DredgionInstance.java:326-328
runtime::Ptr<InstanceScore> DredgionInstance::getInstanceScore() {
	return instanceScore.get();
}

// Java DredgionInstance.java:331-333
void DredgionInstance::leaveInstance(Player& player) {
	TeleportService::moveToInstanceExit(player, mapId, player.getRace());
}

// Java DredgionInstance.java:336-342
void DredgionInstance::portToStartPosition(Player& player) {
	if ((player.getRace() == Race::ELYOS && raceStartPosition == 0) || (player.getRace() == Race::ASMODIANS && raceStartPosition != 0)) {
		TeleportService::teleportTo(player, instance->getMapId(), instance->getInstanceId(), 570.468f, 166.897f, 432.28986f);
	} else {
		TeleportService::teleportTo(player, instance->getMapId(), instance->getInstanceId(), 400.741f, 166.713f, 432.290f);
	}
}

} // namespace aion::gameserver::handlers::instance::dredgion
