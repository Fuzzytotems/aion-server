#pragma once

#include "aion/gameserver/handlers/instance/pvparenas/HarmonyTrainingGroundsInstance.h"

namespace aion::gameserver::handlers::instance::pvparenas {

/**
 * Arena of Harmony (300450000): the Harmony Training Grounds with rewards by rank and difficulty.
 * <p>
 * Java: data/handlers/instance/pvparenas/ArenaOfHarmonyInstance.java, @InstanceID(300450000).
 *
 * @author xTz, Estrayl
 */
class ArenaOfHarmonyInstance : public HarmonyTrainingGroundsInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit ArenaOfHarmonyInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<ArenaOfHarmonyInstance> create(WorldMapInstance& instance);

protected:
	BaseRewards getBaseRewardsPerPlayer(int32_t difficultyId) override;

	BaseRewards getBaseRewards(int32_t difficultyId) override;

	float getRewardRate(int32_t rank, int32_t difficultyId) override;

	void setRewardItems(PvPArenaPlayerReward& reward, int32_t rank, int32_t difficultyId) override;

	float getConfigRate(runtime::Ptr<Player> player) override;
};

} // namespace aion::gameserver::handlers::instance::pvparenas
