#pragma once

#include "aion/gameserver/handlers/instance/pvparenas/ChaosTrainingGroundsInstance.h"

namespace aion::gameserver::handlers::instance::pvparenas {

/**
 * Arena of Chaos (300350000): the Chaos Training Grounds with rewards by rank and difficulty.
 * <p>
 * Java: data/handlers/instance/pvparenas/ArenaOfChaosInstance.java, @InstanceID(300350000).
 *
 * @author xTz, Estrayl
 */
class ArenaOfChaosInstance : public ChaosTrainingGroundsInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit ArenaOfChaosInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<ArenaOfChaosInstance> create(WorldMapInstance& instance);

protected:
	BaseRewards getBaseRewardsPerPlayer(int32_t difficultyId) override;

	BaseRewards getBaseRewards(int32_t difficultyId) override;

	float getRewardRate(int32_t rank, int32_t difficultyId) override;

	void setRewardItems(PvPArenaPlayerReward& reward, int32_t rank, int32_t difficultyId) override;

	float getConfigRate(runtime::Ptr<Player> player) override;
};

} // namespace aion::gameserver::handlers::instance::pvparenas
