#include "aion/gameserver/handlers/instance/pvparenas/PvPArenaInstance.h"

#include <cmath>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/DamageInfo.h"
#include "aion/gameserver/controllers/attack/DamageList.h"
#include "aion/gameserver/controllers/attack/TeamDamageList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/model/autogroup/AGPlayer.h"
#include "aion/gameserver/model/gameobjects/StaticDoor.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/instance/instancescore/PvPArenaScore.h"
#include "aion/gameserver/model/instance/playerreward/HarmonyGroupReward.h"
#include "aion/gameserver/model/instance/playerreward/PvPArenaPlayerReward.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/rewards/ArenaRewardItem.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/services/abyss/GloryPointsService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/DispelCategoryType.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance::pvparenas {

using skillengine::model::DispelCategoryType;

// Java PvPArenaInstance.java:59-61
PvPArenaInstance::PvPArenaInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

// Java PvPArenaInstance.java:63-65
runtime::Ref<PvPArenaScore> PvPArenaInstance::createNewArenaScore() {
	return PvPArenaScore::create(*instance);
}

// Java PvPArenaInstance.java:67-71
void PvPArenaInstance::setScoreCaps() {
	instanceScore.get()->setLowerScoreCap(7000);
	instanceScore.get()->setUpperScoreCap(50000);
	instanceScore.get()->setMaxScoreGap(43000);
}

// Java PvPArenaInstance.java:73-81
void PvPArenaInstance::onInstanceCreate() {
	instanceScore = createNewArenaScore();
	setScoreCaps();
	instanceScore.get()->setInstanceProgressionType(InstanceProgressionType::PREPARING);
	instanceScore.get()->setInstanceStartTime();
	spawnRings();
	instanceTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { startInstance(); }, START_DELAY));
}

// Java PvPArenaInstance.java:83-98
void PvPArenaInstance::startInstance() {
	if (instanceScore.get()->isRewarded())
		return;

	if (!canStart()) {
		isInterrupted = true;
		endInstance();
		return;
	}

	instance->forEachDoor([](model::gameobjects::StaticDoor& door) { door.setOpen(true); });
	instanceScore.get()->setInstanceProgressionType(InstanceProgressionType::START_PROGRESS);
	broadcastUpdate(InstanceScoreType::UPDATE_INSTANCE_PROGRESS);

	instanceTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { startNextRound(); }, ROUND_DURATION));
}

// Java PvPArenaInstance.java:100-117
void PvPArenaInstance::startNextRound() {
	if (instanceScore.get()->isRewarded())
		return;
	if (isFinalRound()) {
		endInstance();
		return;
	}

	instanceScore.get()->incrementRound();
	instanceScore.get()->setRndZone();
	broadcastUpdate(InstanceScoreType::UPDATE_INSTANCE_PROGRESS);
	changeZone();

	if (isFinalRound())
		sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_START_SCOREMOD());

	instanceTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { startNextRound(); }, ROUND_DURATION));
}

// Java PvPArenaInstance.java:119-127
void PvPArenaInstance::endInstance() {
	instance->forEachNpc([](Npc& npc) { npc.getController().delete_(); });
	instanceScore.get()->setInstanceProgressionType(InstanceProgressionType::END_PROGRESS);
	instanceTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		instance->forEachPlayer([this](Player& player) { leaveInstance(player); });
	}, 60000));

	calculateRewards();
	reward();
	broadcastResults();
}

// Java PvPArenaInstance.java:129-142
bool PvPArenaInstance::onDie(Player& victim, Creature& lastAttacker) {
	runtime::Ptr<PvPArenaPlayerReward> victimReward = getStatReward(victim);
	runtime::Ptr<PvPArenaPlayerReward> winnerReward = nullptr;
	runtime::Ptr<Player> winner = runtime::as<Player>(runtime::Ptr<Creature>(lastAttacker));
	if (&lastAttacker != &victim && winner != nullptr) {
		winnerReward = getStatReward(*winner);
		winnerReward->addPvPKill();
		// Notify Kill-Quests
		QuestEngine::getInstance().onKillInWorld(*QuestEnv::create(runtime::Ptr<VisibleObject>(victim), *winner, 0), winner->getWorldId());
	}
	calculateAndUpdatePoints(victim, static_cast<int32_t>(std::floor(static_cast<float>(pointsPerKill.get())
		* getRunnerUpScoreMod(victimReward, winnerReward) + 0.5f))); // Java Math.round(float)
	updatePoints(victimReward, victim, lastAttacker, pointsPerDeath.get(), false); // Update victim points after rewarding the winner
	applyMoraleBoost(victim);
	return true;
}

// Java PvPArenaInstance.java:144-155
void PvPArenaInstance::onDie(Npc& npc) {
	int32_t npcId = npc.getNpcId();
	if (npcId == 701187 || npcId == 701188)
		ThreadPoolManager::getInstance().schedule({this}, [this] { spawnRndRelics(); }, 30000);

	int32_t points = getPoints(npcId);
	if (points == 0)
		return;

	calculateAndUpdatePoints(npc, points);
}

// Java PvPArenaInstance.java:158-175
void PvPArenaInstance::calculateAndUpdatePoints(Creature& victim, int32_t points) {
	if (!instanceScore.get()->isStartProgress())
		return;

	DamageList damageList = victim.getAggroList().getFinalDamageList();
	int32_t totalDamage = damageList.getTotalDamage();
	std::vector<DamageInfo> damages =
		shouldMergeGroupDamage() ? damageList.toTeamDamages().getCreatureOrTeamDamages() : damageList.getCreatureDamages();
	for (const DamageInfo& damageInfo : damages) {
		int32_t rewardPoints = points * damageInfo.getDamage() / totalDamage;
		if (runtime::Ptr<Player> attacker = runtime::as<Player>(damageInfo.getAttacker())) {
			updatePoints(getStatReward(*attacker), *attacker, victim, rewardPoints);
		} else if (runtime::Ptr<TemporaryPlayerTeam> team = runtime::as<TemporaryPlayerTeam>(damageInfo.getAttacker())) {
			runtime::Ptr<Player> leader = team->getLeaderObject();
			updatePoints(getStatReward(*leader), *leader, victim, rewardPoints);
			for (const runtime::Ptr<Player>& p : team->getOnlineMembers())
				if (p.get() != leader.get())
					sendSystemMsg(*p, victim, rewardPoints);
		}
	}
}

// Java PvPArenaInstance.java:177-189
void PvPArenaInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	runtime::Ptr<PvPArenaPlayerReward> arenaReward = getStatReward(*player);
	if (!instanceScore.get()->isStartProgress() || arenaReward == nullptr)
		return;

	int32_t rewardPoints = getPoints(npc.getNpcId());
	int32_t skill = instanceScore.get()->getNpcBonusSkill(npc.getNpcId());
	if (skill != 0)
		useSkill(npc, *player, skill >> 8, skill & 0xFF);

	updatePoints(arenaReward, *player, npc, rewardPoints);
}

// Java PvPArenaInstance.java:191-193
void PvPArenaInstance::updatePoints(runtime::Ptr<PvPArenaPlayerReward> receiver, Player& player, VisibleObject& victim, int32_t rewardPoints) {
	updatePoints(receiver, player, victim, rewardPoints, true);
}

// Java PvPArenaInstance.java:195-209
void PvPArenaInstance::updatePoints(runtime::Ptr<PvPArenaPlayerReward> receiver, Player& player, VisibleObject& victim, int32_t rewardPoints,
                                    bool broadcastPackets) {
	if (receiver == nullptr)
		return;

	receiver->addPoints(rewardPoints, *instanceScore.get());

	if (broadcastPackets) {
		broadcastUpdate(InstanceScoreType::UPDATE_INSTANCE_BUFFS_AND_SCORE);
		broadcastUpdate(player, InstanceScoreType::UPDATE_PLAYER_BUFF_STATUS);
	}
	sendSystemMsg(player, victim, rewardPoints);

	if (instanceScore.get()->reachedScoreGap())
		endInstance();
}

// Java PvPArenaInstance.java:211-213
bool PvPArenaInstance::isFinalRound() {
	return instanceScore.get()->getRound() == 3;
}

// Java PvPArenaInstance.java:215-222
void PvPArenaInstance::sendSystemMsg(Player& player, VisibleObject& creature, int32_t rewardPoints) {
	if (rewardPoints <= 0) // Checked with retail, only net gain is sent as msg
		return;
	if (runtime::as<Player>(runtime::Ptr<VisibleObject>(creature)) != nullptr)
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE_FOR_ENEMY(rewardPoints));
	else
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE(creature.getObjectTemplate()->getL10n(), rewardPoints));
}

// Java PvPArenaInstance.java:224-226
int32_t PvPArenaInstance::calcAndFloor(int32_t number, float rate) {
	return static_cast<int32_t>(static_cast<float>(number) * rate);
}

// Java PvPArenaInstance.java:228-230
int32_t PvPArenaInstance::calcAndFloor(int32_t number, float rate, float configRate) {
	return static_cast<int32_t>(static_cast<float>(number) * rate * configRate);
}

// Java PvPArenaInstance.java:232-234
std::vector<runtime::Ptr<PvPArenaPlayerReward>> PvPArenaInstance::getArenaRewards() {
	return instanceScore.get()->getPlayerRewards();
}

// Java PvPArenaInstance.java:236-327
int32_t PvPArenaInstance::getPoints(int32_t npcId) {
	switch (npcId) {
		case 701187: // Blessed Relics
		case 701188: // Cursed Relics
			return 1750;
		case 218690: // Pale Carmina
		case 218703: // Pale Carmina
		case 218716: // Pale Carmina
		case 218691: // Corrupt Casus
		case 218704: // Corrupt Casus
		case 218717: // Corrupt Casus
			return 1500;
		case 218682: // MuMu Rake Gatherer
		case 218695: // MuMu Rake Gatherer
		case 218708: // MuMu Rake Gatherer
			return 1250;
		case 701181: // Cursed Relics
		case 701195: // Cursed Relics
		case 701209: // Cursed Relics
		case 218689: // Casus Manor Noble
		case 218702: // Casus Manor Noble
		case 218715: // Casus Manor Noble
			return 750;
		case 218701: // Casus Manor Butler
		case 218688: // Casus Manor Butler
		case 218714: // Casus Manor Butler
			return 650;
		case 701172: // Plaza Flame Thrower
		case 701171: // Plaza Flame Thrower
		case 701170: // Plaza Flame Thrower
		case 701169: // Plaza Flame Thrower
		case 218806: // Casus Manor Chief Maid
		case 218807: // Casus Manor Chief Maid
		case 218808: // Casus Manor Chief Maid
		case 701317: // Blessed Relics
		case 701318: // Blessed Relics
		case 701319: // Blessed Relics
			return 500;
		case 207102: // Recovery Relics
		case 207116: // 1st Floor Stunner
		case 219277: // Roaming Volcanic Petrahulk Lv 50
		case 219278: // Roaming Volcanic Petrahulk Lv 55
		case 219279: // Roaming Volcanic Petrahulk Lv 60
		case 219481: // Volcanic Heart Crystal Lv 50
		case 219485: // Volcanic Heart Crystal Lv 55
		case 219486: // Volcanic Heart Crystal Lv 60
		case 219648: // Roaming Volcanic Petrahulk Lv 65
		case 219652: // Volcanic Heart Crystal Lv 65
			return 400;
		case 218685: // Casus Manor Guard
		case 218698: // Casus Manor Guard
		case 218711: // Casus Manor Guard
		case 218687: // Casus Manor Maid
		case 218700: // Casus Manor Maid
		case 218713: // Casus Manor Maid
		case 218686: // Casus Manor Maidservant
		case 218699: // Casus Manor Maidservant
		case 218712: // Casus Manor Maidservant
			return 250;
		case 207099: // 2nd Floor Bomb
			return 200;
		case 218683: // Black Claw Scratcher
		case 218696: // Black Claw Scratcher
		case 218709: // Black Claw Scratcher
		case 218684: // Mutated Drakan Fighter
		case 218697: // Mutated Drakan Fighter
		case 218710: // Mutated Drakan Fighter
		case 218693: // Red Sand Tog
		case 218706: // Red Sand Tog
		case 218719: // Red Sand Tog
		case 219280: // Heated Negotiator Grangvolkan Lv 50
		case 219281: // Heated Negotiator Grangvolkan Lv 55
		case 219282: // Heated Negotiator Grangvolkan Lv 60
		case 219649: // Heated Negotiator Grangvolkan Lv 65
		case 233058: // Red Sand Tog
		case 701215: // Blesed Relic
		case 701220: // Blesed Relic
		case 701225: // Blesed Relic
		case 701216: // Blesed Relic
		case 701221: // Blesed Relic
		case 701226: // Blesed Relic
			return 100;
		case 219283: // Lurking Fangwing Lv 50
		case 219284: // Lurking Fangwing Lv 55
		case 219285: // Lurking Fangwing Lv 60
		case 219650: // Lurking Fangwing Lv 65
		case 219328: // Plaza Wall
			return 50;
		default:
			return 0;
	}
}

// Java PvPArenaInstance.java:329-340
void PvPArenaInstance::changeZone() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		for (const runtime::Ptr<Player>& player : instance->getPlayersInside()) {
			if (player->isDead()) {
				PlayerReviveService::revive(*player, 100, 100, false, 0);
				player->getGameStats()->updateStatsAndSpeedVisually();
			}
			instanceScore.get()->portToPosition(*player);
			broadcastUpdate(*player, InstanceScoreType::UPDATE_PLAYER_BUFF_STATUS);
		}
	}, 1000);
}

// Java PvPArenaInstance.java:342-351
void PvPArenaInstance::onEnterInstance(Player& player) {
	if (instanceScore.get()->regPlayerReward(player)) {
		instanceScore.get()->setRndPosition(player.getObjectId());
	} else {
		instanceScore.get()->portToPosition(player);
		getPlayerSpecificReward(player)->updateBonusTime();
	}
	sendEntryPacket(player);
}

// Java PvPArenaInstance.java:353-356
void PvPArenaInstance::onPlayerLogout(Player& player) {
	getPlayerSpecificReward(player)->updateLogoutTime();
}

// Java PvPArenaInstance.java:358-364
void PvPArenaInstance::clearDebuffs(Player& player) {
	for (const runtime::Ptr<Effect>& ef : player.getEffectController()->getAbnormalEffects()) {
		switch (ef->getSkillTemplate()->getDispelCategory()) {
			case DispelCategoryType::DEBUFF:
			case DispelCategoryType::DEBUFF_MENTAL:
			case DispelCategoryType::DEBUFF_PHYSICAL:
			case DispelCategoryType::ALL:
				ef->endEffect();
				break;
			default:
				break;
		}
	}
}

// Java PvPArenaInstance.java:366-368
void PvPArenaInstance::useSkill(Npc& npc, Player& player, int32_t skillId, int32_t level) {
	SkillEngine::getInstance().getSkill(npc, skillId, level, runtime::Ptr<VisibleObject>(player))->useNoAnimationSkill();
}

// Java PvPArenaInstance.java:370-377
bool PvPArenaInstance::onReviveEvent(Player& player) {
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	PlayerReviveService::revive(player, 100, 100, false, 0);
	player.getGameStats()->updateStatsAndSpeedVisually();
	instanceScore.get()->portToPosition(player);
	return true;
}

// Java PvPArenaInstance.java:379-383
void PvPArenaInstance::leaveInstance(Player& player) {
	if (player.getWorldMapInstance().get() == instance.get())
		TeleportService::moveToInstanceExit(player, mapId, player.getRace());
}

// Java PvPArenaInstance.java:385-396
void PvPArenaInstance::onLeaveInstance(Player& player) {
	GeneralInstanceHandler::onLeaveInstance(player);
	clearDebuffs(player);
	runtime::Ptr<PvPArenaPlayerReward> playerReward = getPlayerSpecificReward(player);
	if (playerReward != nullptr) {
		endMoraleBoost(player);
		instanceScore.get()->clearPosition(playerReward->getPosition(), false);
		instanceScore.get()->removePlayerReward(*playerReward);
		broadcastUpdate(InstanceScoreType::UPDATE_INSTANCE_BUFFS_AND_SCORE);
	}
}

// Java PvPArenaInstance.java:398-403
void PvPArenaInstance::onInstanceDestroy() {
	instanceScore.get()->clear();
	if (runtime::FutureRef task = instanceTask.get(); task != nullptr && !task->isCancelled())
		task->cancel(true);
}

// Java PvPArenaInstance.java:405-412
void PvPArenaInstance::applyMoraleBoost(Player& player) {
	runtime::Ptr<PvPArenaPlayerReward> reward = getPlayerSpecificReward(player);
	if (reward == nullptr)
		return;

	reward->applyBoostMoraleEffect(player, getBoostMoraleEffectDuration(instanceScore.get()->getRank(*reward)));
	broadcastUpdate(player, InstanceScoreType::UPDATE_PLAYER_BUFF_STATUS);
}

// Java PvPArenaInstance.java:414-420
void PvPArenaInstance::endMoraleBoost(Player& player) {
	runtime::Ptr<PvPArenaPlayerReward> reward = getPlayerSpecificReward(player);
	if (reward != nullptr && reward->hasBoostMorale()) {
		reward->endBoostMoraleEffect(player);
	}
	broadcastUpdate(player, InstanceScoreType::UPDATE_PLAYER_BUFF_STATUS);
}

// Java PvPArenaInstance.java:422-425
void PvPArenaInstance::spawnRndRelics() {
	if (instanceScore.get() != nullptr && !instanceScore.get()->isRewarded())
		spawn(commons::utils::Rnd::get(1, 2) == 1 ? 701187 : 701188, 1841.951f, 1733.968f, 300.242f, int8_t{0});
}

// Java PvPArenaInstance.java:427-430
runtime::Ptr<InstanceScore> PvPArenaInstance::getInstanceScore() {
	return instanceScore.get();
}

// Java PvPArenaInstance.java:435-437
runtime::Ptr<PvPArenaPlayerReward> PvPArenaInstance::getStatReward(Player& player) {
	return getPlayerSpecificReward(player);
}

// Java PvPArenaInstance.java:439-441
runtime::Ptr<PvPArenaPlayerReward> PvPArenaInstance::getPlayerSpecificReward(Player& player) {
	return instanceScore.get()->getPlayerReward(player.getObjectId());
}

// Java PvPArenaInstance.java:443-445
bool PvPArenaInstance::canStart() {
	return instance->getPlayersInside().size() > 1;
}

// Java PvPArenaInstance.java:447-448
void PvPArenaInstance::spawnRings() {
}

// Java PvPArenaInstance.java:450-456
float PvPArenaInstance::getRunnerUpScoreMod(runtime::Ptr<PvPArenaPlayerReward> victim, runtime::Ptr<PvPArenaPlayerReward> winner) {
	if (winner == nullptr || !isFinalRound())
		return 1.0f;

	int32_t victimRank = instanceScore.get()->getRank(*victim);
	return victimRank < instanceScore.get()->getRank(*winner) ? getRunnerUpScoreMod(victimRank) : 1.0f;
}

// Java PvPArenaInstance.java:458-460
int32_t PvPArenaInstance::getBoostMoraleEffectDuration(int32_t rank) {
	return 15000; // Retail Default
}

// Java PvPArenaInstance.java:462-464
float PvPArenaInstance::getRunnerUpScoreMod(int32_t victimRank) {
	return 3.0f;
}

// Java PvPArenaInstance.java:466-468
bool PvPArenaInstance::shouldMergeGroupDamage() {
	return false;
}

// Java PvPArenaInstance.java:470-472
PvPArenaInstance::BaseRewards PvPArenaInstance::getBaseRewardsPerPlayer(int32_t difficultyId) {
	return BaseRewards{0, 0, 0, 0};
}

// Java PvPArenaInstance.java:474-476
PvPArenaInstance::BaseRewards PvPArenaInstance::getBaseRewards(int32_t difficultyId) {
	return BaseRewards{0, 0, 0, 0};
}

// Java PvPArenaInstance.java:478-480
float PvPArenaInstance::getRewardRate(int32_t rank, int32_t difficultyId) {
	return 0.0f;
}

// Java PvPArenaInstance.java:482-483
void PvPArenaInstance::setRewardItems(PvPArenaPlayerReward& reward, int32_t rank, int32_t difficultyId) {
}

// Java PvPArenaInstance.java:485-487
float PvPArenaInstance::getConfigRate(runtime::Ptr<Player> player) {
	return 1.0f;
}

// Java PvPArenaInstance.java:489-560
void PvPArenaInstance::calculateRewards() {
	int32_t difficultyId = instanceScore.get()->getDifficultyId();
	BaseRewards baseValues = getBaseRewardsPerPlayer(difficultyId);

	int32_t playerCount = static_cast<int32_t>(instanceScore.get()->getPlayerRewards().size());
	int32_t totalAp = baseValues.ap() * playerCount;
	int32_t totalGp = baseValues.gp() * playerCount;
	int32_t totalCrucibleInsignia = baseValues.crucibleInsignia() * playerCount;
	int32_t totalCourageInsignia = baseValues.courageInsignia() * playerCount;

	int32_t rankAp = calcAndFloor(totalAp, RANK_REWARD_RATE);
	int32_t rankGp = calcAndFloor(totalGp, RANK_REWARD_RATE);
	int32_t rankCrucibleInsignia = calcAndFloor(totalCrucibleInsignia, RANK_REWARD_RATE);
	int32_t rankCourageInsignia = calcAndFloor(totalCourageInsignia, RANK_REWARD_RATE);

	int32_t scoreAp = calcAndFloor(totalAp, SCORE_REWARD_RATE);
	int32_t scoreGp = calcAndFloor(totalGp, SCORE_REWARD_RATE);
	int32_t scoreCrucibleInsignia = calcAndFloor(totalCrucibleInsignia, SCORE_REWARD_RATE);
	int32_t scoreCourageInsignia = calcAndFloor(totalCourageInsignia, SCORE_REWARD_RATE);

	int32_t totalPoints = instanceScore.get()->getTotalPoints();

	for (const runtime::Ptr<PvPArenaPlayerReward>& reward : getArenaRewards()) {
		if (reward->isRewarded())
			continue;

		runtime::Ptr<Player> player = instance->getPlayer(reward->getOwnerId());
		if (runtime::as<HarmonyGroupReward>(reward) == nullptr && player == nullptr) // player left
			continue;

		float configRate = getConfigRate(player);

		int32_t score = reward->getScorePoints();
		// Score Formula to verify: floor(scoreAp * winnerKills * winnerPoints / (winnerKills * winnerPoints + loserKills * loserPoints))
		float scoreRate = static_cast<float>(score) / static_cast<float>(totalPoints);

		int32_t rank = instanceScore.get()->getRank(*reward);
		if (isInterrupted.get())
			rank = runtime::as<HarmonyGroupReward>(reward) != nullptr ? 1 : instance->getMaxPlayers() - 1;
		float rankRewardRate = getRewardRate(rank, difficultyId);

		int32_t rankRewardAp = calcAndFloor(rankAp, rankRewardRate, configRate);
		int32_t rankRewardGp = calcAndFloor(rankGp, rankRewardRate, 1.0f);
		int32_t rankRewardCrucibleInsignia = calcAndFloor(rankCrucibleInsignia, rankRewardRate, configRate);
		int32_t rankRewardCourageInsignia = calcAndFloor(rankCourageInsignia, rankRewardRate, configRate);

		int32_t scoreRewardAp = calcAndFloor(scoreAp, scoreRate, configRate);
		int32_t scoreRewardGp = calcAndFloor(scoreGp, scoreRate, 1.0f);
		int32_t scoreRewardCrucibleInsignia = calcAndFloor(scoreCrucibleInsignia, scoreRate, configRate);
		int32_t scoreRewardCourageInsignia = calcAndFloor(scoreCourageInsignia, scoreRate, configRate);

		BaseRewards baseRewards = getBaseRewards(difficultyId);
		reward->setAp(*ArenaRewardItem::create(0, baseRewards.ap(), rankRewardAp, scoreRewardAp));
		reward->setGp(*ArenaRewardItem::create(0, baseRewards.gp(), rankRewardGp, scoreRewardGp));

		runtime::Ref<ArenaRewardItem> crucibleInsignia =
			ArenaRewardItem::create(186000130, baseRewards.crucibleInsignia(), rankRewardCrucibleInsignia, scoreRewardCrucibleInsignia);
		runtime::Ref<ArenaRewardItem> courageInsignia =
			ArenaRewardItem::create(186000137, baseRewards.courageInsignia(), rankRewardCourageInsignia, scoreRewardCourageInsignia);

		if (crucibleInsignia->getTotalCount() > 0)
			reward->setCrucibleInsignia(*crucibleInsignia);
		if (courageInsignia->getTotalCount() > 0)
			reward->setCourageInsignia(*courageInsignia);

		setRewardItems(*reward, rank, difficultyId);

		if (runtime::Ptr<HarmonyGroupReward> harmonyGroupReward = runtime::as<HarmonyGroupReward>(reward)) {
			splitGroupRewards(*harmonyGroupReward);
		}
	}
}

// Java PvPArenaInstance.java:562-583
void PvPArenaInstance::splitGroupRewards(HarmonyGroupReward& groupReward) {
	std::vector<runtime::Ptr<AGPlayer>> associatedPlayers = groupReward.getAssociatedPlayers();
	if (associatedPlayers.empty())
		return;

	int32_t size = static_cast<int32_t>(associatedPlayers.size());

	for (const runtime::Ptr<AGPlayer>& agPlayer : associatedPlayers) {
		runtime::Ptr<PvPArenaPlayerReward> apr = instanceScore.get()->getPlayerReward(agPlayer->objectId());
		runtime::Ptr<Player> player = instance->getPlayer(agPlayer->objectId());
		if (apr == nullptr || player == nullptr)
			continue;

		float configRate = getConfigRate(player);
		apr->setAp(*calculateIndividualReward(*groupReward.getAp(), size, configRate));
		apr->setGp(*calculateIndividualReward(*groupReward.getGp(), size, 1.0f));
		apr->setCrucibleInsignia(*calculateIndividualReward(*groupReward.getCrucibleInsignia(), size, configRate));
		apr->setCourageInsignia(*calculateIndividualReward(*groupReward.getCourageInsignia(), size, configRate));
		apr->setRewardItem1(groupReward.getRewardItem1());
		apr->setRewardItem2(groupReward.getRewardItem2());
	}
}

// Java PvPArenaInstance.java:585-590 (Java Math.round(float): floor(x + 0.5f))
runtime::Ref<ArenaRewardItem> PvPArenaInstance::calculateIndividualReward(ArenaRewardItem& rewardItem, int32_t size, float configRate) {
	int32_t baseCount = static_cast<int32_t>(std::floor(static_cast<float>(rewardItem.baseCount()) * configRate / static_cast<float>(size) + 0.5f));
	int32_t rankingCount =
		static_cast<int32_t>(std::floor(static_cast<float>(rewardItem.rankingCount()) * configRate / static_cast<float>(size) + 0.5f));
	int32_t scoreCount = static_cast<int32_t>(std::floor(static_cast<float>(rewardItem.scoreCount()) * configRate / static_cast<float>(size) + 0.5f));
	return ArenaRewardItem::create(rewardItem.itemId(), baseCount, rankingCount, scoreCount);
}

// Java PvPArenaInstance.java:592-625
void PvPArenaInstance::reward() {
	for (const runtime::Ptr<Player>& player : instance->getPlayersInside()) {
		if (player->isDead())
			PlayerReviveService::duelRevive(*player);
		runtime::Ptr<PvPArenaPlayerReward> reward = getPlayerSpecificReward(*player);
		if (!reward->isRewarded()) {
			reward->setRewarded();

			runtime::Ptr<ArenaRewardItem> apReward = reward->getAp();
			if (apReward->getTotalCount() > 0)
				AbyssPointsService::addAp(*player, apReward->getTotalCount());

			runtime::Ptr<ArenaRewardItem> gpReward = reward->getGp();
			if (gpReward->getTotalCount() > 0)
				GloryPointsService::addGp(player->getObjectId(), gpReward->getTotalCount());

			runtime::Ptr<ArenaRewardItem> courageInsigniaReward = reward->getCourageInsignia();
			if (courageInsigniaReward->getTotalCount() > 0)
				ItemService::addItem(*player, courageInsigniaReward->itemId(), courageInsigniaReward->getTotalCount());

			runtime::Ptr<ArenaRewardItem> crucibleInsigniaReward = reward->getCrucibleInsignia();
			if (crucibleInsigniaReward->getTotalCount() > 0)
				ItemService::addItem(*player, crucibleInsigniaReward->itemId(), crucibleInsigniaReward->getTotalCount());

			runtime::Ptr<RewardItem> rewardItem1 = reward->getRewardItem1();
			if (rewardItem1 != nullptr)
				ItemService::addItem(*player, rewardItem1->getId(), rewardItem1->getCount());

			runtime::Ptr<RewardItem> rewardItem2 = reward->getRewardItem2();
			if (rewardItem2 != nullptr)
				ItemService::addItem(*player, rewardItem2->getId(), rewardItem2->getCount());
		}
	}
}

// Java PvPArenaInstance.java:627-634
runtime::Ptr<Npc> PvPArenaInstance::getNpc(float x, float y, float z) {
	for (const runtime::Ptr<Npc>& npc : instance->getNpcs()) {
		runtime::Ptr<model::templates::spawns::SpawnTemplate> st = npc->getSpawn();
		if (st->getX() == x && st->getY() == y && st->getZ() == z)
			return npc;
	}
	return nullptr;
}

// Java PvPArenaInstance.java:636-638
void PvPArenaInstance::sendEntryPacket(Player& player) {
	sendPacket(nullptr, std::nullopt);
}

// Java PvPArenaInstance.java:640-642
void PvPArenaInstance::broadcastUpdate(Player& target, InstanceScoreType scoreType) {
	sendPacket(nullptr, std::nullopt);
}

// Java PvPArenaInstance.java:644-646
void PvPArenaInstance::broadcastUpdate(InstanceScoreType scoreType) {
	sendPacket(nullptr, std::nullopt);
}

// Java PvPArenaInstance.java:648-650
void PvPArenaInstance::broadcastResults() {
	instance->forEachPlayer([this](Player& player) { sendPacket(runtime::Ptr<Player>(player), InstanceScoreType::SHOW_REWARD); });
}

// Java PvPArenaInstance.java:652-653
void PvPArenaInstance::sendPacket(runtime::Ptr<Player> receiver, std::optional<InstanceScoreType> scoreType) {
}

// Java PvPArenaInstance.java:655-658
bool PvPArenaInstance::allowSelfReviveBySkill() {
	return false;
}

// Java PvPArenaInstance.java:660-663
bool PvPArenaInstance::allowSelfReviveByItem() {
	return false;
}

} // namespace aion::gameserver::handlers::instance::pvparenas
