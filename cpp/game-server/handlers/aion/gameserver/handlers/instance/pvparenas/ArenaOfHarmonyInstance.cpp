#include "aion/gameserver/handlers/instance/pvparenas/ArenaOfHarmonyInstance.h"

#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/instance/playerreward/PvPArenaPlayerReward.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"

namespace aion::gameserver::handlers::instance::pvparenas {

AION_INSTANCE_HANDLER(ArenaOfHarmonyInstance, 300450000);

// Java ArenaOfHarmonyInstance.java:17-19
ArenaOfHarmonyInstance::ArenaOfHarmonyInstance(WorldMapInstance& instance) : HarmonyTrainingGroundsInstance(instance) {
}

runtime::Ref<ArenaOfHarmonyInstance> ArenaOfHarmonyInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<ArenaOfHarmonyInstance>(instance);
}

// Java ArenaOfHarmonyInstance.java:21-29
PvPArenaInstance::BaseRewards ArenaOfHarmonyInstance::getBaseRewardsPerPlayer(int32_t difficultyId) {
	switch (difficultyId) {
		case 2:
			return BaseRewards{2200, 0, 0, 22};
		case 3:
			return BaseRewards{3200, 0, 0, 34};
		case 4:
			return BaseRewards{2500, 31, 0, 34};
		default:
			return BaseRewards{1200, 0, 0, 12};
	}
}

// Java ArenaOfHarmonyInstance.java:31-34
PvPArenaInstance::BaseRewards ArenaOfHarmonyInstance::getBaseRewards(int32_t difficultyId) {
	return BaseRewards{200, 0, 0, 0};
}

// Java ArenaOfHarmonyInstance.java:36-39
float ArenaOfHarmonyInstance::getRewardRate(int32_t rank, int32_t difficultyId) {
	return rank == 0 ? 0.9f : 0.1f;
}

// Java ArenaOfHarmonyInstance.java:41-62
void ArenaOfHarmonyInstance::setRewardItems(PvPArenaPlayerReward& reward, int32_t rank, int32_t difficultyId) {
	switch (rank) {
		case 0: {
			switch (difficultyId) {
				case 1:
					reward.setRewardItem1(RewardItem::create(188052179, 1)); // Lesser Arena Supply Package
					break;
				case 2:
					reward.setRewardItem1(RewardItem::create(188052180, 1)); // Arena Supply Package
					break;
				case 3:
					reward.setRewardItem1(RewardItem::create(188052185, 1)); // Composite Manastone Pouch
					break;
				case 4: {
					reward.setRewardItem1(RewardItem::create(188052605, 1)); // Victor's Reward Box
					reward.setRewardItem2(RewardItem::create(188052482, 1)); // Superior Arena Supply Package
					break;
				}
			}
			break;
		}
		case 1: {
			switch (difficultyId) {
				case 3:
					reward.setRewardItem1(RewardItem::create(188052181, 1)); // Greater Arena Supply Package
					break;
				case 4:
					reward.setRewardItem1(RewardItem::create(188052606, 1)); // Consolation Prize Box
					break;
			}
			break;
		}
	}
}

// Java ArenaOfHarmonyInstance.java:64-69
float ArenaOfHarmonyInstance::getConfigRate(runtime::Ptr<Player> player) {
	if (player == nullptr) // Happens during calculation for group rewards
		return 1.0f;
	return model::gameobjects::player::get(*player, *RatesConfig::PVP_ARENA_HARMONY_REWARD_RATES.get());
}

} // namespace aion::gameserver::handlers::instance::pvparenas
