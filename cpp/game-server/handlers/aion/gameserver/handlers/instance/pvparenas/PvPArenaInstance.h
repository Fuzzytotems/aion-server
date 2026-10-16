#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance::pvparenas {

/**
 * The base of the PvP arenas (Discipline, Chaos, Glory, Harmony and their training grounds): two minutes of preparation, three rounds of
 * three minutes in random zones, the points for kills, deaths, npcs and objects, the end at the score gap or after the last round, the
 * rank and score rewards.
 * <p>
 * Java: data/handlers/instance/pvparenas/PvPArenaInstance.java (abstract: not registered). C++: the scheduled lambdas are pinned to the
 * handler; Java's record BaseRewards is a struct; Java's `sendPacket(null, null)` calls pass std::nullopt for the score type.
 *
 * @author xTz, Estrayl
 */
class PvPArenaInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	static constexpr int32_t START_DELAY = 120000;
	static constexpr int32_t ROUND_DURATION = 180000;

protected:
	static constexpr float RANK_REWARD_RATE = 0.7f;
	static constexpr float SCORE_REWARD_RATE = 0.3f;

	runtime::Field<int32_t> pointsPerKill{800};
	runtime::Field<int32_t> pointsPerDeath{-150};

	runtime::Field<runtime::Ref<PvPArenaScore>> instanceScore{};

private:
	runtime::Field<runtime::FutureRef> instanceTask{};
	runtime::Field<bool> isInterrupted{};

protected:
	/** Java: protected record BaseRewards(int ap, int gp, int crucibleInsignia, int courageInsignia) */
	struct BaseRewards {
		int32_t ap_;
		int32_t gp_;
		int32_t crucibleInsignia_;
		int32_t courageInsignia_;

		int32_t ap() const { return ap_; }
		int32_t gp() const { return gp_; }
		int32_t crucibleInsignia() const { return crucibleInsignia_; }
		int32_t courageInsignia() const { return courageInsignia_; }
	};

	explicit PvPArenaInstance(WorldMapInstance& instance);

	virtual runtime::Ref<PvPArenaScore> createNewArenaScore();

	virtual void setScoreCaps();

public:
	void onInstanceCreate() override;

private:
	void startInstance();

	void startNextRound();

	void endInstance();

public:
	bool onDie(Player& victim, Creature& lastAttacker) override;

	void onDie(Npc& npc) override;

private:
	void calculateAndUpdatePoints(Creature& victim, int32_t points);

public:
	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

protected:
	virtual void updatePoints(runtime::Ptr<PvPArenaPlayerReward> receiver, Player& player, VisibleObject& victim, int32_t rewardPoints);

	virtual void updatePoints(runtime::Ptr<PvPArenaPlayerReward> receiver, Player& player, VisibleObject& victim, int32_t rewardPoints,
	                          bool broadcastPackets);

	virtual bool isFinalRound();

	virtual void sendSystemMsg(Player& player, VisibleObject& creature, int32_t rewardPoints);

private:
	int32_t calcAndFloor(int32_t number, float rate);

	int32_t calcAndFloor(int32_t number, float rate, float configRate);

protected:
	virtual std::vector<runtime::Ptr<PvPArenaPlayerReward>> getArenaRewards();

private:
	int32_t getPoints(int32_t npcId);

	void changeZone();

public:
	void onEnterInstance(Player& player) override;

	void onPlayerLogout(Player& player) override;

private:
	void clearDebuffs(Player& player);

protected:
	void useSkill(Npc& npc, Player& player, int32_t skillId, int32_t level);

public:
	bool onReviveEvent(Player& player) override;

	void leaveInstance(Player& player) override;

	void onLeaveInstance(Player& player) override;

	void onInstanceDestroy() override;

private:
	void applyMoraleBoost(Player& player);

	void endMoraleBoost(Player& player);

	void spawnRndRelics();

public:
	runtime::Ptr<InstanceScore> getInstanceScore() override;

protected:
	/** Overridden in HarmonyTrainingGroundsInstance to differentiate between player and group specific reward objects */
	virtual runtime::Ptr<PvPArenaPlayerReward> getStatReward(Player& player);

	virtual runtime::Ptr<PvPArenaPlayerReward> getPlayerSpecificReward(Player& player);

	virtual bool canStart();

	virtual void spawnRings();

	virtual float getRunnerUpScoreMod(runtime::Ptr<PvPArenaPlayerReward> victim, runtime::Ptr<PvPArenaPlayerReward> winner);

	virtual int32_t getBoostMoraleEffectDuration(int32_t rank);

	virtual float getRunnerUpScoreMod(int32_t victimRank);

	virtual bool shouldMergeGroupDamage();

	virtual BaseRewards getBaseRewardsPerPlayer(int32_t difficultyId);

	virtual BaseRewards getBaseRewards(int32_t difficultyId);

	virtual float getRewardRate(int32_t rank, int32_t difficultyId);

	virtual void setRewardItems(PvPArenaPlayerReward& reward, int32_t rank, int32_t difficultyId);

	virtual float getConfigRate(runtime::Ptr<Player> player);

	virtual void calculateRewards();

private:
	void splitGroupRewards(HarmonyGroupReward& groupReward);

	runtime::Ref<ArenaRewardItem> calculateIndividualReward(ArenaRewardItem& rewardItem, int32_t size, float configRate);

	void reward();

protected:
	runtime::Ptr<Npc> getNpc(float x, float y, float z);

	using GeneralInstanceHandler::getNpc;

	virtual void sendEntryPacket(Player& player);

	virtual void broadcastUpdate(Player& target, InstanceScoreType scoreType);

	virtual void broadcastUpdate(InstanceScoreType scoreType);

	virtual void broadcastResults();

	/** Java sendPacket(Player receiver, InstanceScoreType scoreType): both null from the default broadcasts */
	virtual void sendPacket(runtime::Ptr<Player> receiver, std::optional<InstanceScoreType> scoreType);

public:
	bool allowSelfReviveBySkill() override;

	bool allowSelfReviveByItem() override;
};

} // namespace aion::gameserver::handlers::instance::pvparenas
