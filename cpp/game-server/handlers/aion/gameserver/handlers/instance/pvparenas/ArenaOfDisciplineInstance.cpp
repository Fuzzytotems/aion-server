#include "aion/gameserver/handlers/instance/pvparenas/ArenaOfDisciplineInstance.h"

#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/instance/playerreward/PvPArenaPlayerReward.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"

namespace aion::gameserver::handlers::instance::pvparenas {

AION_INSTANCE_HANDLER(ArenaOfDisciplineInstance, 300360000);

// Java ArenaOfDisciplineInstance.java:17-19
ArenaOfDisciplineInstance::ArenaOfDisciplineInstance(WorldMapInstance& instance) : DisciplineTrainingGroundsInstance(instance) {
}

runtime::Ref<ArenaOfDisciplineInstance> ArenaOfDisciplineInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<ArenaOfDisciplineInstance>(instance);
}

// Java ArenaOfDisciplineInstance.java:21-30
PvPArenaInstance::BaseRewards ArenaOfDisciplineInstance::getBaseRewardsPerPlayer(int32_t difficultyId) {
	// Extracted values from instant_dungeon_idarenapvp.xml (Retail Leak 4.6, also verified by different videos)
	switch (difficultyId) {
		case 2:
			return BaseRewards{900, 0, 425, 38};
		case 3:
			return BaseRewards{1100, 0, 655, 59};
		case 4:
			return BaseRewards{1200, 39, 655, 59};
		default:
			return BaseRewards{700, 0, 310, 20};
	}
}

// Java ArenaOfDisciplineInstance.java:32-39
PvPArenaInstance::BaseRewards ArenaOfDisciplineInstance::getBaseRewards(int32_t difficultyId) {
	switch (difficultyId) {
		case 1:
			return BaseRewards{200, 0, 90, 0};
		case 2:
			return BaseRewards{200, 0, 125, 0};
		default:
			return BaseRewards{200, 0, 195, 0};
	}
}

// Java ArenaOfDisciplineInstance.java:41-47
float ArenaOfDisciplineInstance::getRewardRate(int32_t rank, int32_t difficultyId) {
	switch (rank) {
		case 0:
			return difficultyId == 4 ? 0.75f : 0.72f;
		default:
			return difficultyId == 4 ? 0.25f : 0.28f;
	}
}

// Java ArenaOfDisciplineInstance.java:49-53
void ArenaOfDisciplineInstance::setRewardItems(PvPArenaPlayerReward& reward, int32_t rank, int32_t difficultyId) {
	if (rank != 0 && difficultyId > 1)
		reward.setRewardItem1(RewardItem::create(186000165, 4)); // Opportunity Token
}

// Java ArenaOfDisciplineInstance.java:55-58
float ArenaOfDisciplineInstance::getConfigRate(runtime::Ptr<Player> player) {
	return model::gameobjects::player::get(*player, *RatesConfig::PVP_ARENA_DISCIPLINE_REWARD_RATES.get());
}

} // namespace aion::gameserver::handlers::instance::pvparenas
