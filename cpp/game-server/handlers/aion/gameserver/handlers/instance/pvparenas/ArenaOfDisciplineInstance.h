#pragma once

#include "aion/gameserver/handlers/instance/pvparenas/DisciplineTrainingGroundsInstance.h"

namespace aion::gameserver::handlers::instance::pvparenas {

/**
 * Arena of Discipline (300360000): the Discipline Training Grounds with rewards by difficulty.
 * <p>
 * Java: data/handlers/instance/pvparenas/ArenaOfDisciplineInstance.java, @InstanceID(300360000).
 *
 * @author xTz, Estrayl
 */
class ArenaOfDisciplineInstance : public DisciplineTrainingGroundsInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit ArenaOfDisciplineInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<ArenaOfDisciplineInstance> create(WorldMapInstance& instance);

protected:
	BaseRewards getBaseRewardsPerPlayer(int32_t difficultyId) override;

	BaseRewards getBaseRewards(int32_t difficultyId) override;

	float getRewardRate(int32_t rank, int32_t difficultyId) override;

	void setRewardItems(PvPArenaPlayerReward& reward, int32_t rank, int32_t difficultyId) override;

	float getConfigRate(runtime::Ptr<Player> player) override;
};

} // namespace aion::gameserver::handlers::instance::pvparenas
