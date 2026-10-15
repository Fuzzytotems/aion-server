#pragma once

#include "aion/gameserver/handlers/instance/pvparenas/PvPArenaInstance.h"

namespace aion::gameserver::handlers::instance::pvparenas {

/**
 * Arena of Glory (300550000): 1000 points a kill, -200 a death, rewards by rank and difficulty.
 * <p>
 * Java: data/handlers/instance/pvparenas/ArenaOfGloryInstance.java, @InstanceID(300550000).
 *
 * @author xTz, Estrayl
 */
class ArenaOfGloryInstance : public PvPArenaInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit ArenaOfGloryInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<ArenaOfGloryInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

protected:
	int32_t getBoostMoraleEffectDuration(int32_t rank) override;

	float getRunnerUpScoreMod(int32_t victimRank) override;

	using PvPArenaInstance::getRunnerUpScoreMod;

	void sendPacket(runtime::Ptr<Player> player, std::optional<InstanceScoreType> scoreType) override;

protected:
	BaseRewards getBaseRewardsPerPlayer(int32_t difficultyId) override;

	float getRewardRate(int32_t rank, int32_t difficultyId) override;

	void setRewardItems(PvPArenaPlayerReward& reward, int32_t rank, int32_t difficultyId) override;

	float getConfigRate(runtime::Ptr<Player> player) override;
};

} // namespace aion::gameserver::handlers::instance::pvparenas
