#include "aion/gameserver/handlers/instance/pvp/BasicPvpInstance.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/instance/instancescore/PvpInstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/PvpInstancePlayerReward.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/network/aion/instanceinfo/PvpInstanceScoreWriter.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/services/abyss/GloryPointsService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/stats/AbyssRankEnumInfo.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include <string>

namespace aion::gameserver::handlers::instance::pvp {

// Java BasicPvpInstance.java:49-51
BasicPvpInstance::BasicPvpInstance(WorldMapInstance& instance)
	: GeneralInstanceHandler(instance), raceStartPosition(commons::utils::Rnd::nextInt(2)) {
}

runtime::Ref<BasicPvpInstance> BasicPvpInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<BasicPvpInstance>(instance);
}

// Java BasicPvpInstance.java:53-63
void BasicPvpInstance::onInstanceCreate() {
	if (instanceScore.get() == nullptr)
		return;

	instanceScore.get()->addPointsByRace(Race::ELYOS, 1000);
	instanceScore.get()->addPointsByRace(Race::ASMODIANS, 1000);
	updateProgress(InstanceProgressionType::REINFORCE_MEMBER);
	spawnFactionRelatedNpcs();
	tasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] { startPreparation(); }, getReinforceMemberPhaseDelay()));
}

// Java BasicPvpInstance.java:65-66
void BasicPvpInstance::spawnFactionRelatedNpcs() {
}

// Java BasicPvpInstance.java:68-71
void BasicPvpInstance::startPreparation() {
	updateProgress(InstanceProgressionType::PREPARING);
	tasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] { onStart(); }, 60000));
}

// Java BasicPvpInstance.java:73-74
void BasicPvpInstance::onStart() {
}

// Java BasicPvpInstance.java:76-85
void BasicPvpInstance::onStop(bool isBossKilled) {
	cancelTasks();
	updateProgress(InstanceProgressionType::END_PROGRESS);

	Race winningRace = instanceScore.get()->getRaceWithHighestPoints();
	instance->forEachPlayer([this, winningRace, isBossKilled](Player& p) {
		setAndDistributeRewards(p, *runtime::cast<PvpInstancePlayerReward>(instanceScore.get()->getPlayerReward(p.getObjectId())), winningRace,
			isBossKilled);
	});
	instance->forEachNpc([](Npc& npc) { npc.getController().delete_(); });
	tasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] {
		for (const runtime::Ptr<Player>& player : instance->getPlayersInside())
			revivePlayerOnEnd(*player);
	}, 10000));
	tasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] {
		for (const runtime::Ptr<Player>& player : instance->getPlayersInside())
			leaveInstance(*player);
	}, 60000));
}

// Java BasicPvpInstance.java:87-89
void BasicPvpInstance::setAndDistributeRewards(Player& player, PvpInstancePlayerReward& reward, Race winningRace, bool isBossKilled) {
}

// Java BasicPvpInstance.java:91-108
void BasicPvpInstance::distributeRewards(Player& player, PvpInstancePlayerReward& reward) {
	runtime::Ref<PvpInstanceScoreWriter> writer =
		PvpInstanceScoreWriter::create(*instanceScore.get(), InstanceScoreType::SHOW_REWARD, player.getObjectId(), 0);
	PacketSendUtility::sendPacket(player, SM_INSTANCE_SCORE(instance->getMapId(), *writer, getTime()));
	AbyssPointsService::addAp(player, model::gameobjects::player::calcResult(Rates::AP_DREDGION, player, reward.getBaseAp() + reward.getBonusAp()));
	int32_t gpToAdd = reward.getBaseGp() + reward.getBonusGp();
	if (gpToAdd > 0)
		GloryPointsService::addGp(player.getObjectId(), model::gameobjects::player::calcResult(Rates::GP, player, gpToAdd));
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

// Java BasicPvpInstance.java:110-114
void BasicPvpInstance::leaveInstance(Player& player) {
	if (player.getWorldMapInstance().get() == instance.get())
		TeleportService::moveToInstanceExit(player, mapId, player.getRace());
}

// Java BasicPvpInstance.java:116-124
void BasicPvpInstance::onLeaveInstance(Player& player) {
	GeneralInstanceHandler::onLeaveInstance(player);
	runtime::Ptr<PvpInstancePlayerReward> reward = runtime::cast<PvpInstancePlayerReward>(instanceScore.get()->getPlayerReward(player.getObjectId()));
	if (reward != nullptr && instanceScore.get()->getInstanceProgressionType() != InstanceProgressionType::END_PROGRESS)
		instanceScore.get()->removePlayerReward(*reward);
	sendScore(PvpInstanceScoreWriter::create(*instanceScore.get(), InstanceScoreType::PLAYER_QUIT, player.getObjectId(), 0));
}

// Java BasicPvpInstance.java:127-144 (`killPoints *= 1.6f`: Java's compound assignment narrows the float product to int)
bool BasicPvpInstance::onDie(Player& victim, Creature& lastAttacker) {
	sendScore(PvpInstanceScoreWriter::create(*instanceScore.get(), InstanceScoreType::UPDATE_PLAYER_BUFF_STATUS, victim.getObjectId(), 60));
	runtime::Ptr<Player> killer = runtime::as<Player>(runtime::Ptr<Creature>(lastAttacker));
	if (killer != nullptr && killer->getRace() != victim.getRace()) {
		int32_t killPoints = 200;
		if (instanceScore.get()->isStartProgress() && getTime() <= 600000 && victim.getRace() != instanceScore.get()->getRaceWithHighestPoints())
			killPoints += 100; // After 10 minutes the outplayed faction gets bonus points

		if (utils::stats::id(victim.getAbyssRank()->getRank()) - utils::stats::id(killer->getAbyssRank()->getRank()) >= 4)
			killPoints = static_cast<int32_t>(static_cast<float>(killPoints) * 1.6f);

		updatePoints(killer, killer->getRace(), std::nullopt, killPoints, false, true);
	}

	updatePoints(runtime::Ptr<Player>(victim), victim.getRace(), std::nullopt, -100, true, false);
	return true;
}

// Java BasicPvpInstance.java:146-155
bool BasicPvpInstance::onReviveEvent(Player& player) {
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	PlayerReviveService::revive(player, 100, 100, false, 0);
	player.getGameStats()->updateStatsAndSpeedVisually();
	portToStartPosition(player);
	sendScore(PvpInstanceScoreWriter::create(*instanceScore.get(), InstanceScoreType::UPDATE_PLAYER_BUFF_STATUS, player.getObjectId(), 0));
	return true;
}

// Java BasicPvpInstance.java:157-168
void BasicPvpInstance::onEnterInstance(Player& player) {
	if (!instanceScore.get()->containsPlayer(player.getObjectId()))
		instanceScore.get()->addPlayerReward(*PvpInstancePlayerReward::create(player.getObjectId(), player.getRace()));

	sendScore(PvpInstanceScoreWriter::create(*instanceScore.get(), InstanceScoreType::INIT_PLAYER, player.getObjectId(), 0));
	sendScore(PvpInstanceScoreWriter::create(*instanceScore.get(), InstanceScoreType::UPDATE_INSTANCE_BUFFS_AND_SCORE, playersInside()));
	sendScore(PvpInstanceScoreWriter::create(*instanceScore.get(), InstanceScoreType::UPDATE_PLAYER_BUFF_STATUS, player.getObjectId(), 0));
}

// Java BasicPvpInstance.java:170-173
void BasicPvpInstance::onInstanceDestroy() {
	cancelTasks();
}

// Java BasicPvpInstance.java:175-177
int32_t BasicPvpInstance::getReinforceMemberPhaseDelay() {
	return 60000;
}

// Java BasicPvpInstance.java:179-186
int32_t BasicPvpInstance::getTime() {
	int32_t current = static_cast<int32_t>(commons::utils::currentTimeMillis() - startTime.get());
	switch (instanceScore.get()->getInstanceProgressionType()) {
		case InstanceProgressionType::REINFORCE_MEMBER:
			return 120000 - current;
		case InstanceProgressionType::PREPARING:
			return 60000 - current;
		case InstanceProgressionType::START_PROGRESS:
		case InstanceProgressionType::END_PROGRESS:
			return 1200000 - current;
	}
	// Java: the switch expression over all constants has no default; javac's synthetic default throws MatchException
	throw runtime::IllegalStateException("MatchException: " + std::to_string(static_cast<int32_t>(instanceScore.get()->getInstanceProgressionType())));
}

// Java BasicPvpInstance.java:188-197
void BasicPvpInstance::updateProgress(InstanceProgressionType progressionType) {
	instanceScore.get()->setInstanceProgressionType(progressionType);
	startTime = commons::utils::currentTimeMillis(); // Reset start time
	sendScore(PvpInstanceScoreWriter::create(*instanceScore.get(), InstanceScoreType::UPDATE_INSTANCE_PROGRESS));
	sendScore(PvpInstanceScoreWriter::create(*instanceScore.get(), InstanceScoreType::UPDATE_ALL_PLAYER_INFO, playersInside()));
	sendScore(PvpInstanceScoreWriter::create(*instanceScore.get(), InstanceScoreType::UPDATE_INSTANCE_BUFFS_AND_SCORE, playersInside()));
}

// Java BasicPvpInstance.java:199-201
void BasicPvpInstance::updatePoints(runtime::Ptr<Player> player, Race race, std::optional<std::string_view> npcL10n, int32_t points) {
	updatePoints(player, race, npcL10n, points, false, false);
}

// Java BasicPvpInstance.java:203-227
void BasicPvpInstance::updatePoints(runtime::Ptr<Player> player, Race race, std::optional<std::string_view> npcL10n, int32_t points, bool isVictim,
                                    bool shouldRewardPvpKill) {
	if (!instanceScore.get()->isStartProgress())
		return;

	instanceScore.get()->addPointsByRace(race, points);
	if (player != nullptr) {
		runtime::Ptr<PvpInstancePlayerReward> pReward =
			runtime::cast<PvpInstancePlayerReward>(instanceScore.get()->getPlayerReward(player->getObjectId()));
		pReward->addPoints(points);
		if (isVictim) {
			sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_LOSE_SCORE_ENEMY(player->getName(), utils::ChatUtil::l10n(model::getL10nId(race)), points));
		} else {
			if (shouldRewardPvpKill) {
				instanceScore.get()->incrementKillsByRace(race);
				pReward->addPvPKill();
				PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE_FOR_ENEMY(points));
			} else if (npcL10n.has_value()) {
				PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE(*npcL10n, points));
			}
		}
	}
	sendScore(PvpInstanceScoreWriter::create(*instanceScore.get(), InstanceScoreType::UPDATE_FACTION_SCORE, race));
	sendScore(PvpInstanceScoreWriter::create(*instanceScore.get(), InstanceScoreType::UPDATE_ALL_PLAYER_INFO, playersInside()));
}

// Java BasicPvpInstance.java:229-232
void BasicPvpInstance::revivePlayerOnEnd(Player& player) {
	if (player.isDead())
		PlayerReviveService::duelRevive(player);
}

// Java BasicPvpInstance.java:238-242
void BasicPvpInstance::cancelTasks() {
	for (const runtime::FutureRef& task : tasks)
		if (task != nullptr && !task->isCancelled())
			task->cancel(true);
}

void BasicPvpInstance::sendScore(runtime::Ref<PvpInstanceScoreWriter> writer) {
	sendPacket(SM_INSTANCE_SCORE(instance->getMapId(), *writer, getTime()));
}

std::vector<runtime::Ref<Player>> BasicPvpInstance::playersInside() {
	std::vector<runtime::Ref<Player>> players;
	for (const runtime::Ptr<Player>& player : instance->getPlayersInside())
		players.emplace_back(*player);
	return players;
}

// Java BasicPvpInstance.java:244-247
runtime::Ptr<InstanceScore> BasicPvpInstance::getInstanceScore() {
	return instanceScore.get();
}

} // namespace aion::gameserver::handlers::instance::pvp
