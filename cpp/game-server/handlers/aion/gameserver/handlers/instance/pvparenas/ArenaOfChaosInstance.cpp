#include "aion/gameserver/handlers/instance/pvparenas/ArenaOfChaosInstance.h"

#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/instance/playerreward/PvPArenaPlayerReward.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"

namespace aion::gameserver::handlers::instance::pvparenas {

AION_INSTANCE_HANDLER(ArenaOfChaosInstance, 300350000);

// Java ArenaOfChaosInstance.java:17-19
ArenaOfChaosInstance::ArenaOfChaosInstance(WorldMapInstance& instance) : ChaosTrainingGroundsInstance(instance) {
}

runtime::Ref<ArenaOfChaosInstance> ArenaOfChaosInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<ArenaOfChaosInstance>(instance);
}

// Java ArenaOfChaosInstance.java:21-30
PvPArenaInstance::BaseRewards ArenaOfChaosInstance::getBaseRewardsPerPlayer(int32_t difficultyId) {
	// Extracted values from instant_dungeon_idarenapvp.xml (Retail Leak 4.6, also verified by different videos)
	switch (difficultyId) {
		case 2:
			return BaseRewards{900, 0, 425, 50};
		case 3:
			return BaseRewards{1100, 0, 655, 77};
		case 4:
			return BaseRewards{1300, 31, 655, 77};
		default:
			return BaseRewards{700, 0, 310, 27};
	}
}

// Java ArenaOfChaosInstance.java:32-39
PvPArenaInstance::BaseRewards ArenaOfChaosInstance::getBaseRewards(int32_t difficultyId) {
	switch (difficultyId) {
		case 1:
			return BaseRewards{200, 0, 90, 0};
		case 2:
			return BaseRewards{200, 0, 125, 0};
		default:
			return BaseRewards{200, 0, 195, 0};
	}
}

// Java ArenaOfChaosInstance.java:41-52
float ArenaOfChaosInstance::getRewardRate(int32_t rank, int32_t difficultyId) {
	switch (rank) {
		case 0:
			return 0.21f;
		case 1:
			return 0.16f;
		case 2:
			return 0.135f;
		case 3:
			return 0.115f;
		case 4:
			return 0.095f;
		case 5:
			return 0.075f;
		default:
			return 0.050f;
	}
}

// Java ArenaOfChaosInstance.java:54-66
void ArenaOfChaosInstance::setRewardItems(PvPArenaPlayerReward& reward, int32_t rank, int32_t difficultyId) {
	switch (rank) {
		case 0:
		case 1: {
			if (difficultyId > 2)
				reward.setRewardItem2(RewardItem::create(186000185, 1)); // Arena of Glory Ticket, Item2 is indented by retail
			break;
		}
		case 2:
			reward.setRewardItem1(RewardItem::create(186000165, 1)); // Opportunity Token
			break;
		case 3:
			reward.setRewardItem1(RewardItem::create(186000165, 2)); // Opportunity Token
			break;
		case 4:
			reward.setRewardItem1(RewardItem::create(186000165, 3)); // Opportunity Token
			break;
		default:
			reward.setRewardItem1(RewardItem::create(186000165, 5)); // Opportunity Token
			break;
	}
}

// Java ArenaOfChaosInstance.java:68-71
float ArenaOfChaosInstance::getConfigRate(runtime::Ptr<Player> player) {
	return model::gameobjects::player::get(*player, *RatesConfig::PVP_ARENA_CHAOS_REWARD_RATES.get());
}

} // namespace aion::gameserver::handlers::instance::pvparenas
